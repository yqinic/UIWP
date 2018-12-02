#include "contiki.h"
#include "net/rime/rime.h"
#include "sys/ctimer.h"
#include <stdlib.h>
#include <stdio.h>

#define DEBUG 0
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#define DEBUG_RELIABLE 0

#define COMMAND_CHANNEL CHANNEL_OFFSET

PROCESS(sensor_process, "Sensor Process");
AUTOSTART_PROCESSES(&sensor_process);

static struct broadcast_conn bc;
static struct unicast_conn uc;
static struct bunicast_conn buc;

static linkaddr_t addr;

#define DATA_SIZE 96
char buf[DATA_SIZE];

// broadcast communication
static void
recv_bc(struct broadcast_conn *c, const linkaddr_t *from)
{
    PRINTF("broadcast message received from %d.%d\n",
	    from->u8[0], from->u8[1]);

    // acknowledgment frame

    uint8_t buf[UIWPACK_FRAME_LENGTH];
    int num = 32;
    uiwpack_frame_create(num, buf);

    PRINTF("send ack frame\n");

    packetbuf_copyfrom(buf, UIWPACK_FRAME_LENGTH);
    unicast_send(&uc, from);
}

static void
sent_bc(struct broadcast_conn *c, int status, int num_tx)
{
    PRINTF("broadcast message sent: status %d num_tx %d\n", status, num_tx);
}

static const struct broadcast_callbacks broadcast_callbacks = {recv_bc, sent_bc};

// unicast communication
static void
recv_uc(struct unicast_conn *c, const linkaddr_t *from)
{

    PRINTF("unicast message received from %d.%d\n", from->u8[0], from->u8[1]);
    linkaddr_copy(&addr, from);

    // data request frame received
    // bunicast data to sink

    struct uiwprequest_list uiwprequestlist;
    uiwprequest_frame_parse((uint8_t *)packetbuf_dataptr(), &uiwprequestlist);
    PRINTF("prepare switching to channel: %d\n", uiwprequestlist.channel);

    if (channel_switch(uiwprequestlist.channel) == CHANNEL_SWITCH_OK) {
        PRINTF("bunicast send to sink, channel: %d\n", uiwprequestlist.channel);

        // char *buf = "hello world\n";
        bunicast_size(&buc, 1);
        bunicast_send(&buc, from, buf);

        // cannot channel switch here
    } else
        PRINTF("failed to switch to channel %d\n", uiwprequestlist.channel);
}

static void
sent_uc(struct unicast_conn *c, int status, int num_tx)
{
    const linkaddr_t *dest = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);
    if(linkaddr_cmp(dest, &linkaddr_null)) {
        return;
    }
    PRINTF("unicast message sent to %d.%d: status %d num_tx %d\n",
        dest->u8[0], dest->u8[1], status, num_tx);
}

static const struct unicast_callbacks unicast_callbacks = {recv_uc, sent_uc};

// bunicast communication
static void
recv_buc(struct bunicast_conn *c, const linkaddr_t *from)
{
    printf("bunicast message received from %d.%d, msg: %s\n",
	    from->u8[0], from->u8[1], (char *)packetbuf_dataptr());
}

static void
sent_buc(struct bunicast_conn *c, int status)
{

    // const linkaddr_t *dest = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);
    PRINTF("bunicast message sent to %d.%d: status %d\n",
        addr.u8[0], addr.u8[1], status);

#if DEBUG_RELIABLE
    static int burst_sent_count = 0;

    burst_sent_count += 1;

    if (burst_sent_count == 72) {
        channel_switch(COMMAND_CHANNEL);
        burst_sent_count = 0;
    }
    else 
        bunicast_send(c, &addr, buf);
#else
    // more data to be sent write in here
    channel_switch(COMMAND_CHANNEL);
#endif
}

static const struct bunicast_callbacks bunicast_callbacks = {recv_buc, sent_buc};

static void
com_init()
{
    broadcast_open(&bc, 127, &broadcast_callbacks);
    unicast_open(&uc, 146, &unicast_callbacks);
    bunicast_open(&buc, 135, &bunicast_callbacks);
    channel_switch(COMMAND_CHANNEL);
}

PROCESS_THREAD(sensor_process, ev, data)
{
    PROCESS_BEGIN();

    com_init();

    static struct etimer et;

    int i;
    for (i = 0; i < DATA_SIZE; i++) {
        buf[i] = 'a';
    }

    while(1) {
        etimer_set(&et, CLOCK_SECOND * 40);

        // sensor applications to be put here 

        PROCESS_WAIT_UNTIL(etimer_expired(&et));
    }

    PROCESS_END();
}