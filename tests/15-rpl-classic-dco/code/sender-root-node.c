/*
 * Copyright (c) 2012, Thingsquare, www.thingsquare.com.
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
 */


#include "contiki.h"
#include "lib/random.h"
#include "sys/ctimer.h"
#include "sys/etimer.h"
#include "net/ipv6/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ipv6/uip-debug.h"
#include "net/routing/routing.h"
#include "simple-udp.h"

#include <stdio.h>
#include <string.h>

#define RECEIVE_PORT 1234
#define SEND_PORT 4321
#define SERVICE_ID 190


#define SEND_INTERVAL		(20 * CLOCK_SECOND)

/* Start sending messages START_DELAY secs after we start so that routing can
 * converge */
#define START_DELAY 500

#define ITERATIONS 180


static struct simple_udp_connection receive_connection;
static struct simple_udp_connection send_connection;

/*---------------------------------------------------------------------------*/
PROCESS(unicast_receiver_process, "Unicast receiver example process");
AUTOSTART_PROCESSES(&unicast_receiver_process);
/*---------------------------------------------------------------------------*/
static void
receiver(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{
  printf("Root;Receiving;");
  uip_debug_ipaddr_print(sender_addr);
  printf(";%s\n",data);
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(unicast_receiver_process, ev, data)
{

  static struct etimer send_timer;
  uip_ipaddr_t addr;

  static int address_selection = 0;

  static unsigned int message_number;

  PROCESS_BEGIN();

  NETSTACK_ROUTING.root_start();

  simple_udp_register(&receive_connection, RECEIVE_PORT,
                      NULL, RECEIVE_PORT, receiver);
  simple_udp_register(&send_connection, SEND_PORT,
                      NULL, SEND_PORT, receiver);

  etimer_set(&send_timer, START_DELAY*CLOCK_SECOND);

  while(1) {

    PROCESS_YIELD();
    if(etimer_expired(&send_timer)) {
      if(message_number == ITERATIONS) {
        etimer_stop(&send_timer);
      } else {
        uip_ip6addr_copy(&addr, uip_ds6_default_prefix());

        if (address_selection %3 == 0) {
          addr.u16[4] = UIP_HTONS(0x0207);
          addr.u16[5] = UIP_HTONS(0x0007);
          addr.u16[6] = UIP_HTONS(0x0007);
          addr.u16[7] = UIP_HTONS(0x0007);
        }
        if (address_selection %3 == 1) {
          addr.u16[4] = UIP_HTONS(0x0208);
          addr.u16[5] = UIP_HTONS(0x0008);
          addr.u16[6] = UIP_HTONS(0x0008);
          addr.u16[7] = UIP_HTONS(0x0008);
        }
        if (address_selection %3 == 2) {
          addr.u16[4] = UIP_HTONS(0x0209);
          addr.u16[5] = UIP_HTONS(0x0009);
          addr.u16[6] = UIP_HTONS(0x0009);
          addr.u16[7] = UIP_HTONS(0x0009);
        }

        {

          char buf[20];

          printf("Root;Sending;");
          uip_debug_ipaddr_print(&addr);
          printf(";%d\n", message_number);
          sprintf(buf, "%d", message_number);
          message_number++;
          address_selection= address_selection + 1;
          simple_udp_sendto(&send_connection, buf, strlen(buf) + 1, &addr);
        }

        etimer_set(&send_timer, SEND_INTERVAL);
      }
    }

    // if(ev == PROCESS_EVENT_TIMER && etimer_expired(&send_timer)) {

    //   uip_ip6addr_copy(&addr, uip_ds6_default_prefix());

    //   if (address_selection %3 == 0) {
    //     addr.u16[4] = UIP_HTONS(0x0207);
    //     addr.u16[5] = UIP_HTONS(0x0007);
    //     addr.u16[6] = UIP_HTONS(0x0007);
    //     addr.u16[7] = UIP_HTONS(0x0007);
    //   }
    //   if (address_selection %3 == 1) {
    //     addr.u16[4] = UIP_HTONS(0x0208);
    //     addr.u16[5] = UIP_HTONS(0x0008);
    //     addr.u16[6] = UIP_HTONS(0x0008);
    //     addr.u16[7] = UIP_HTONS(0x0008);
    //   }
    //   if (address_selection %3 == 2) {
    //     addr.u16[4] = UIP_HTONS(0x0209);
    //     addr.u16[5] = UIP_HTONS(0x0009);
    //     addr.u16[6] = UIP_HTONS(0x0009);
    //     addr.u16[7] = UIP_HTONS(0x0009);
    //   }

    //   {
    //     static unsigned int message_number;
    //     char buf[20];

    //     printf("Root;Sending;");
    //     uip_debug_ipaddr_print(&addr);
    //     printf(";%d\n", message_number);
    //     sprintf(buf, "%d", message_number);
    //     message_number++;
    //     address_selection= address_selection + 1;
    //     simple_udp_sendto(&send_connection, buf, strlen(buf) + 1, &addr);
    //   }


    // }
    // etimer_reset(&send_timer);

  }



  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
