/*
 * Copyright (c) 2010, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \file
 *         A null RDC implementation that uses framer for headers.
 * \author
 *         Adam Dunkels <adam@sics.se>
 *         Niclas Finne <nfi@sics.se>
 */

// Modified by Yuan Qin <yuan.qin14@imperial.ac.uk.

#include "net/mac/mac-sequence.h"
#include "net/mac/nullrdc.h"
#include "net/packetbuf.h"
#include "net/queuebuf.h"
#include "net/netstack.h"
#include "net/rime/rimestats.h"
#include <string.h>

#if CONTIKI_TARGET_COOJA || CONTIKI_TARGET_COOJA_IP64
#include "lib/simEnvChange.h"
#include "sys/cooja_mt.h"
#endif /* CONTIKI_TARGET_COOJA || CONTIKI_TARGET_COOJA_IP64 */

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#ifdef NULLRDC_CONF_ADDRESS_FILTER
#define NULLRDC_ADDRESS_FILTER NULLRDC_CONF_ADDRESS_FILTER
#else
#define NULLRDC_ADDRESS_FILTER 1
#endif /* NULLRDC_CONF_ADDRESS_FILTER */

#ifndef NULLRDC_802154_AUTOACK
#ifdef NULLRDC_CONF_802154_AUTOACK
#define NULLRDC_802154_AUTOACK NULLRDC_CONF_802154_AUTOACK
#else
#define NULLRDC_802154_AUTOACK 1
#endif /* NULLRDC_CONF_802154_AUTOACK */
#endif /* NULLRDC_802154_AUTOACK */

#ifndef NULLRDC_802154_AUTOACK_HW
#ifdef NULLRDC_CONF_802154_AUTOACK_HW
#define NULLRDC_802154_AUTOACK_HW NULLRDC_CONF_802154_AUTOACK_HW
#else
#define NULLRDC_802154_AUTOACK_HW 0
#endif /* NULLRDC_CONF_802154_AUTOACK_HW */
#endif /* NULLRDC_802154_AUTOACK_HW */

#if NULLRDC_802154_AUTOACK
#include "sys/rtimer.h"
#include "dev/watchdog.h"

#ifdef NULLRDC_CONF_ACK_WAIT_TIME
#define ACK_WAIT_TIME NULLRDC_CONF_ACK_WAIT_TIME
#else /* NULLRDC_CONF_ACK_WAIT_TIME */
#define ACK_WAIT_TIME                      RTIMER_SECOND / 2500
#endif /* NULLRDC_CONF_ACK_WAIT_TIME */
#ifdef NULLRDC_CONF_AFTER_ACK_DETECTED_WAIT_TIME
#define AFTER_ACK_DETECTED_WAIT_TIME NULLRDC_CONF_AFTER_ACK_DETECTED_WAIT_TIME
#else /* NULLRDC_CONF_AFTER_ACK_DETECTED_WAIT_TIME */
#define AFTER_ACK_DETECTED_WAIT_TIME       RTIMER_SECOND / 1500
#endif /* NULLRDC_CONF_AFTER_ACK_DETECTED_WAIT_TIME */
#endif /* NULLRDC_802154_AUTOACK */

#ifdef NULLRDC_CONF_SEND_802154_ACK
#define NULLRDC_SEND_802154_ACK NULLRDC_CONF_SEND_802154_ACK
#else /* NULLRDC_CONF_SEND_802154_ACK */
#define NULLRDC_SEND_802154_ACK 0
#endif /* NULLRDC_CONF_SEND_802154_ACK */

#if NULLRDC_SEND_802154_ACK
#include "net/mac/frame802154.h"
#endif /* NULLRDC_SEND_802154_ACK */

#define ACK_LEN 3

#define CCA_CHECK_TIME                     RTIMER_ARCH_SECOND / 8192
#define CCA_SLEEP_TIME                     (RTIMER_ARCH_SECOND / 2000) + 1
#define CHECK_TIME                         (2 * (CCA_CHECK_TIME + CCA_SLEEP_TIME))
#define CYCLE_TIME (RTIMER_ARCH_SECOND / NETSTACK_RDC_CHANNEL_CHECK_RATE)

#define STROBE_TIME                        (CYCLE_TIME + 2 * CHECK_TIME)

/*---------------------------------------------------------------------------*/
static int
send_packet(mac_callback_t sent, void *ptr)
{
  int ret;
  int strobes = 0;
  uint8_t got_strobe_ack = 0;
  rtimer_clock_t t0;
  int len = 0;
  uint8_t seqno = 255;
  uint8_t collisions;
  int last_sent_ok = 0;


  packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &linkaddr_node_addr);
  packetbuf_set_attr(PACKETBUF_ATTR_MAC_ACK, 1);

  if(NETSTACK_FRAMER.create() < 0) {
    /* Failed to allocate space for headers */
    PRINTF("nullrdc: send failed, too large header\n");
    ret = MAC_TX_ERR_FATAL;
  } else{
    int is_broadcast;

    NETSTACK_RADIO.prepare(packetbuf_hdrptr(), packetbuf_totlen());

    is_broadcast = packetbuf_holds_broadcast();

    if(NETSTACK_RADIO.receiving_packet() ||
      (!is_broadcast && NETSTACK_RADIO.pending_packet())) {

        /* Currently receiving a packet over air or the radio has
          already received a packet that needs to be read before
          sending with auto ack. */
      ret = MAC_TX_COLLISION;
    } else {

      if(!is_broadcast) {
        RIMESTATS_ADD(reliabletx);
        seqno = packetbuf_attr(PACKETBUF_ATTR_MAC_SEQNO);
        }

      watchdog_periodic();
      t0 = RTIMER_NOW();

      for(strobes = 0, collisions = 0; 
        got_strobe_ack == 0 && collisions == 0 &&
        RTIMER_CLOCK_LT(RTIMER_NOW(), t0 + STROBE_TIME);
        strobes++) 
      {
            
        watchdog_periodic();

        rtimer_clock_t wt;
        NETSTACK_RADIO.transmit(packetbuf_totlen());

        wt = RTIMER_NOW();
        while(RTIMER_CLOCK_LT(RTIMER_NOW(), wt + ACK_WAIT_TIME)) {}

        if(!is_broadcast && (NETSTACK_RADIO.receiving_packet() ||
                            NETSTACK_RADIO.pending_packet() ||
                            NETSTACK_RADIO.channel_clear() == 0)) {
          uint8_t ackbuf[ACK_LEN];
          wt = RTIMER_NOW();
          while(RTIMER_CLOCK_LT(RTIMER_NOW(), wt + AFTER_ACK_DETECTED_WAIT_TIME)) {}

          len = NETSTACK_RADIO.read(ackbuf, ACK_LEN);
          if(len == ACK_LEN && seqno == ackbuf[ACK_LEN - 1]) {
            got_strobe_ack = 1;
            ret = MAC_TX_OK;
          } else {
            collisions++;
            ret = MAC_TX_COLLISION;
          }
        }
      }

      if(!is_broadcast && !got_strobe_ack && collisions == 0)
        ret = MAC_TX_NOACK;

      if(is_broadcast)
        ret = MAC_TX_OK;
    }
  }

  if(ret == MAC_TX_OK) {
    last_sent_ok = 1;
  }

  mac_call_sent_callback(sent, ptr, ret, 1);
  return last_sent_ok;
}
/*---------------------------------------------------------------------------*/
static void
nullrdc_send_packet(mac_callback_t sent, void *ptr)
{
  send_packet(sent, ptr);
}
/*---------------------------------------------------------------------------*/
static void
send_list(mac_callback_t sent, void *ptr, struct rdc_buf_list *buf_list)
{
  while(buf_list != NULL) {
    /* We backup the next pointer, as it may be nullified by
     * mac_call_sent_callback() */
    struct rdc_buf_list *next = buf_list->next;
    int last_sent_ok;

    queuebuf_to_packetbuf(buf_list->buf);
    last_sent_ok = send_packet(sent, ptr);

    /* If packet transmission was not successful, we should back off and let
     * upper layers retransmit, rather than potentially sending out-of-order
     * packet fragments. */
    if(!last_sent_ok) {
      return;
    }
    buf_list = next;
  }
}
/*---------------------------------------------------------------------------*/
static void
packet_input(void)
{
#if NULLRDC_SEND_802154_ACK
  int original_datalen;
  uint8_t *original_dataptr;

  original_datalen = packetbuf_datalen();
  original_dataptr = packetbuf_dataptr();
#endif

#if NULLRDC_802154_AUTOACK
  if(packetbuf_datalen() == ACK_LEN) {
    /* Ignore ack packets */
    PRINTF("nullrdc: ignored ack\n"); 
  } else
#endif /* NULLRDC_802154_AUTOACK */
  if(NETSTACK_FRAMER.parse() < 0) {
    PRINTF("nullrdc: failed to parse %u\n", packetbuf_datalen());
#if NULLRDC_ADDRESS_FILTER
  } else if(!linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
                                         &linkaddr_node_addr) &&
            !packetbuf_holds_broadcast()) {
    PRINTF("nullrdc: not for us\n");
#endif /* NULLRDC_ADDRESS_FILTER */
  } else {
    int duplicate = 0;

// #if NULLRDC_802154_AUTOACK || NULLRDC_802154_AUTOACK_HW
// #if RDC_WITH_DUPLICATE_DETECTION
    /* Check for duplicate packet. */
    duplicate = mac_sequence_is_duplicate();
    if(duplicate) {
      /* Drop the packet. */
      PRINTF("nullrdc: drop duplicate link layer packet %u\n",
             packetbuf_attr(PACKETBUF_ATTR_MAC_SEQNO));
    } else {
      PRINTF("nullrdc: non duplicated packets detected\n");
      mac_sequence_register_seqno();
    }
// #endif /* RDC_WITH_DUPLICATE_DETECTION */
// #endif /* NULLRDC_802154_AUTOACK */

#if NULLRDC_SEND_802154_ACK
    {
      frame802154_t info154;
      frame802154_parse(original_dataptr, original_datalen, &info154);
      if(info154.fcf.frame_type == FRAME802154_DATAFRAME &&
         info154.fcf.ack_required != 0 &&
         linkaddr_cmp((linkaddr_t *)&info154.dest_addr,
                      &linkaddr_node_addr)) {
        uint8_t ackdata[ACK_LEN] = {0, 0, 0};

        ackdata[0] = FRAME802154_ACKFRAME;
        ackdata[1] = 0;
        ackdata[2] = info154.seq;
        NETSTACK_RADIO.send(ackdata, ACK_LEN);
      }
    }
#endif /* NULLRDC_SEND_ACK */
    if(!duplicate) {
      PRINTF("nullrdc: pass packets to mac layer\n");
      NETSTACK_MAC.input();
    }
  }
}
/*---------------------------------------------------------------------------*/
static int
on(void)
{
  return NETSTACK_RADIO.on();
}
/*---------------------------------------------------------------------------*/
static int
off(int keep_radio_on)
{
  if(keep_radio_on) {
    return NETSTACK_RADIO.on();
  } else {
    return NETSTACK_RADIO.off();
  }
}
/*---------------------------------------------------------------------------*/
static unsigned short
channel_check_interval(void)
{
  return 0;
}
/*---------------------------------------------------------------------------*/
static void
init(void)
{
  on();
}
/*---------------------------------------------------------------------------*/
const struct rdc_driver nullrdc_driver = {
  "nullrdc",
  init,
  nullrdc_send_packet,
  send_list,
  packet_input,
  on,
  off,
  channel_check_interval,
};
/*---------------------------------------------------------------------------*/
