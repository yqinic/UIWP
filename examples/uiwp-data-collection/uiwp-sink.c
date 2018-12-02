#include "contiki.h"
#include "net/rime/rime.h"
#include "sys/ctimer.h"
#include <stdio.h>

#define DEBUG 1
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#define DEBUG_TIMER 0
#define DEBUG_RELIABLE 0

#define CHANNEL_SWITCHING 1

// expire time in clock ticks 
#define UIWP_ACK_EXPIRE 16
#define UIWP_REQUEST_EXPIRE 8
#define UIWP_GLOREQ_EXPIRE 128
#define UIWP_DATA_EXPIRE 16
#define UIWP_GLODATA_EXPIRE 256

#define MAX_ACK_NUM 9

#define COMMAND_CHANNEL CHANNEL_OFFSET

#define EVENT_BC_STOP 0xA0

enum {
    // request timer 
    REQ_TIMER,

    // global request timer
    GLOBALREQ_TIMER,

    // data timer
    DATA_TIMER,

    // global data timer
    GLOBALDATA_TIMER,

    TIMER_NUM
};

PROCESS(sink_process, "Sink Process");
PROCESS(broadcast_process, "Broadcast Process");
AUTOSTART_PROCESSES(&sink_process);

static struct broadcast_conn bc;
static struct unicast_conn uc;
static struct bunicast_conn buc;

static struct ctimer expire_timer[TIMER_NUM];

struct uiwppriority_list *global_uiwpplist;

static int data_channel = 14;

// 0: initialize, no ack is received
// 1: first ack has received
// 2: ack collecting time expires
static volatile int unicast_ack_collecting = 0;

// 0: initialize, no bunicast data is received
// 1: first bunicast data is received
// 2: bunicast data collecting time expires
static volatile int bunicast_data_receiving = 0;

#if DEBUG_TIMER
static int num_ct = 0;
static unsigned long arch_time = 0;
#endif

#if DEBUG_RELIABLE
static int success = 0;
static int fail = 0;
static int packets_count = 0;
static int packets_count_total = 0;
#endif

static void
request_list_runout()
{
    PRINTF("uiwp-sink: priority list run out, start broadcast process\n");

    unicast_ack_collecting = 0;

    ctimer_stop(&expire_timer[REQ_TIMER]);
    ctimer_stop(&expire_timer[GLOBALREQ_TIMER]);

    process_start(&broadcast_process, NULL);
}

static void 
unicast_request()
{
    // create data request frame

    uint8_t buf[UIWPREQUEST_FRAME_LENGTH];
    struct uiwppriority_list *uiwpplist = uiwpplist_get();

    if (uiwpplist == NULL) {

        request_list_runout();
        return;
    }

    // uiwprequest_frame_create(data_channel, 0, uiwpplist->uiwpacklist->bytes, buf);
    uiwprequest_frame_create(data_channel, 0, uiwpplist->uiwpacklist.bytes, buf);


    PRINTF("uiwp-sink: data channel: %d, send to %d.%d\n", 
        data_channel, uiwpplist->addr.u8[0], uiwpplist->addr.u8[1]);

    // send data request frame on command channel
    packetbuf_copyfrom(buf, UIWPREQUEST_FRAME_LENGTH);
    unicast_send(&uc, &uiwpplist->addr);

    // free(buf);

    // // set expire time
    // ctimer_set(&request_timer, UIWP_REQUEST_EXPIRE, request_expire, uiwpplist);
    global_uiwpplist = uiwpplist;
}

static void
request_expire(void *ptr)
{
    PRINTF("uiwp-sink: data request expire, resend from priority list\n");

    // switch back to command channel
    channel_switch(COMMAND_CHANNEL);

    struct uiwppriority_list *uiwpplist = ptr;

    // retransmission + 1
    if (!uiwpplist_retx_update(uiwpplist)) {

        request_list_runout();
        return;
    }

    unicast_request();
}

static void
global_request_expire()
{
    PRINTF("uiwp-sink: global data request expires, back to broadcast\n");

    channel_switch(COMMAND_CHANNEL);

#if DEBUG_RELIABLE
    fail += 1;
#endif
    
    // data request unicast expires, back to broadcast
    // stop request timer, since it may not expire
    ctimer_stop(&expire_timer[REQ_TIMER]);
    // free the priority list
    uiwpplist_free();
    // reset the acknowledgment status
    unicast_ack_collecting = 0;
    // start broadcast session
    process_start(&broadcast_process, NULL);
}

static void
unicast_request_from_ack()
{
    // unicast ack received time expires, disable it
    unicast_ack_collecting = 2;
    PRINTF("uiwp-sink: unicast ack received time expires, data request begins\n");

    // channel scan to obtain optimal data channel
#if CHANNEL_SWITCHING
    // data_channel = channel_scan();
    data_channel = 14;
#else
    data_channel = 26;
#endif

    unicast_request();

    // set global unicast request expire time
    ctimer_set(&expire_timer[GLOBALREQ_TIMER], UIWP_GLOREQ_EXPIRE, global_request_expire, NULL);
}

// broadcast communication
static void
recv_bc(struct broadcast_conn *c, const linkaddr_t *from)
{
    PRINTF("uiwp-sink: broadcast message received from %d.%d\n",
	    from->u8[0], from->u8[1]);
}

static void
sent_bc(struct broadcast_conn *c, int status, int num_tx)
{
    PRINTF("uiwp-sink: broadcast message sent: status %d num_tx %d\n", status, num_tx);
}

static const struct broadcast_callbacks broadcast_callbacks = {recv_bc, sent_bc};

// unicast communication
static void
recv_uc(struct unicast_conn *c, const linkaddr_t *from)
{

    PRINTF("uiwp-sink: unicast message received from %d.%d\n", from->u8[0], from->u8[1]);

    // acknowledgment frame received

    struct uiwpack_list uiwpacklist;
    uiwpack_frame_parse((uint8_t *)packetbuf_dataptr(), &uiwpacklist);

    int rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);

    static int ack_num = 0;
    static struct ctimer ack_timer;

    switch (unicast_ack_collecting) {
        case 0:
            // when first ack frame received
            // free previous priority list
            // set expire time
            // stop broadcasting
            // initialize priority list

            ack_num = 0;
            unicast_ack_collecting = 1;

#if DEBUG_TIMER
            arch_time = rtimer_arch_now();
#endif

            ctimer_set(&ack_timer, UIWP_ACK_EXPIRE, unicast_request_from_ack, NULL);

            // empty the priority list
            uiwpplist_free();
            uiwpplist_create(rssi, from, &uiwpacklist);

            // stop broadcasting
            // process_post(&sink_process, PROCESS_EVENT_MSG, "bcstop");
            process_post(&sink_process, EVENT_BC_STOP, NULL);

            PRINTF("uiwp-sink: first unicast ack received, broadcast stops\n");

            break;
        case 1:
            // add new acknowledged sensor nodes to the list
            uiwpplist_create(rssi, from, &uiwpacklist);

            PRINTF("uiwp-sink: keep receiving unicast ack...\n");

            break;
        default:
            // ack collecting time expires, do nothing
            PRINTF("uiwp-sink: ack collecting time expires, nothing here\n");
    }

    ack_num += 1;
    if (ack_num >= MAX_ACK_NUM) {
        ctimer_stop(&ack_timer);
        unicast_request_from_ack();
    }
}

static void
sent_uc(struct unicast_conn *c, int status, int num_tx)
{
#if DEBUG
    const linkaddr_t *dest = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);
#endif
    PRINTF("uiwp-sink: unicast message sent to %d.%d: status %d num_tx %d\n",
        dest->u8[0], dest->u8[1], status, num_tx);

    if (unicast_ack_collecting == 2) {
        // channel switch
        channel_switch(data_channel);

        // set expire time
        ctimer_set(&expire_timer[REQ_TIMER], UIWP_REQUEST_EXPIRE, request_expire, global_uiwpplist);
    }
    
}

static const struct unicast_callbacks unicast_callbacks = {recv_uc, sent_uc};

static void 
global_data_expire() {
    PRINTF("uiwp-sink: bunicast data session expires\n");

    ctimer_stop(&expire_timer[DATA_TIMER]);
    ctimer_stop(&expire_timer[GLOBALDATA_TIMER]);

    bunicast_data_receiving = 0;

    channel_switch(COMMAND_CHANNEL);

#if DEBUG_RELIABLE 
    fail += 1;
#endif

    process_start(&broadcast_process, NULL);
}

// bunicast communication
static void
recv_buc(struct bunicast_conn *c, const linkaddr_t *from)
{
    PRINTF("uiwp-sink: bunicast message received from %d.%d, msg: %s\n",
	    from->u8[0], from->u8[1], (char *)packetbuf_dataptr());

    // bunicast data received, reset acknowledgment status
    unicast_ack_collecting = 0;

    ctimer_set(&expire_timer[DATA_TIMER], UIWP_DATA_EXPIRE, global_data_expire, NULL);

    switch (bunicast_data_receiving) {

        case 0:
            // stop the data request timer
            ctimer_stop(&expire_timer[REQ_TIMER]);
            ctimer_stop(&expire_timer[GLOBALREQ_TIMER]);

#if DEBUG_TIMER
        num_ct += 1;
        // printf("uiwp-sink: latency: %lu\n", rtimer_arch_now() - arch_time);
        printf("%lu\n", rtimer_arch_now() - arch_time);
        if (num_ct == 200)
            printf("200 number count! \n");
#endif
#if DEBUG_RELIABLE
            packets_count = 1;
            packets_count_total += 1;
#endif

            // bunicast receives the first data
            bunicast_data_receiving = 1;

            // set expire time
            ctimer_set(&expire_timer[GLOBALDATA_TIMER], UIWP_GLODATA_EXPIRE, global_data_expire, NULL);
    
            break;
        case 1:
            PRINTF("uiwp-sink: keep receiving bunicast data ... \n");

#if DEBUG_RELIABLE
            packets_count += 1;
            packets_count_total += 1;

            if (packets_count == 72) {
                success += 1;
                fail -= 1;
                global_data_expire();
            }
#endif
            break;
        default:
            PRINTF("uiwp-sink: bunicast data request time is already expired\n");
    }
    
}

static void
sent_buc(struct bunicast_conn *c, int status)
{
#if DEBUG
    const linkaddr_t *dest = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);
#endif
    PRINTF("uiwp-sink: bunicast message sent to %d.%d: status %d\n",
        dest->u8[0], dest->u8[1], status);
}

static const struct bunicast_callbacks bunicast_callbacks = {recv_buc, sent_buc};

static void
com_init()
{
    broadcast_open(&bc, 127, &broadcast_callbacks);
    unicast_open(&uc, 146, &unicast_callbacks);
    bunicast_open(&buc, 135, &bunicast_callbacks);
}

PROCESS_THREAD(sink_process, ev, data)
{
    PROCESS_BEGIN();

    com_init();

    channel_switch(COMMAND_CHANNEL);

    while(1) {

        PRINTF("uiwp-sink: PROCESS_EVENT_MSG: %d\n", (int)ev);

        switch (ev) {

            case EVENT_BC_STOP:
                process_exit(&broadcast_process);
                PRINTF("uiwp-sink: broadcast terminated\n");
                break;
            case PROCESS_EVENT_INIT:
                process_start(&broadcast_process, NULL);

                break;
            default:
                PRINTF("uiwp-sink: do nothing in switch ev\n");

        }

        PROCESS_YIELD();

        PRINTF("uiwp-sink: process yielded\n");
    }

    PROCESS_END();
}

PROCESS_THREAD(broadcast_process, ev, data)
{
    PROCESS_BEGIN();

    static struct etimer et;

    while(1) {
        etimer_set(&et, CLOCK_SECOND * 5);

        packetbuf_copyfrom("uiwp-sink: data acquire", 12);
        broadcast_send(&bc);

#if DEBUG_RELIABLE
        printf("uiwp-sink: success transmission: %d, fail transmission: %d, total packets received: %d\n", 
            success, fail, packets_count_total);
#endif

        PRINTF("uiwp-sink: broadcast msg sent by sink\n");

        PROCESS_WAIT_UNTIL(etimer_expired(&et));
    }

    PROCESS_END();
}