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

#include "simple-udp.h"

#include <stdio.h>
#include <string.h>

#include "sys/energest.h"

#define UDP_PORT 1234

#define SEND_INTERVAL		(60 * CLOCK_SECOND)


static struct simple_udp_connection unicast_connection;

/*---------------------------------------------------------------------------*/
PROCESS(sender_node_process, "Sender node process");
AUTOSTART_PROCESSES(&sender_node_process);
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
  printf("Sender received data on port %d from port %d with length %d\n",
         receiver_port, sender_port, datalen);
}
/*---------------------------------------------------------------------------*/
static void
set_global_address(void)
{
  uip_ipaddr_t ipaddr;
  int i;
  uint8_t state;
  const uip_ipaddr_t *default_prefix = uip_ds6_default_prefix();

  uip_ip6addr_copy(&ipaddr, default_prefix);
  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
  uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);

  printf("IPv6 addresses: ");
  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(uip_ds6_if.addr_list[i].isused &&
       (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
      uip_debug_ipaddr_print(&uip_ds6_if.addr_list[i].ipaddr);
      printf("\n");
    }
  }
}
/*---------------------------------------------------------------------------*/
static inline unsigned long
to_seconds(uint64_t time)
{
  return (unsigned long)(time / ENERGEST_SECOND);
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(sender_node_process, ev, data)
{
  static struct etimer periodic_timer;

  uip_ipaddr_t addr;
  const uip_ipaddr_t *default_prefix;
  static struct etimer energest_timer;

  PROCESS_BEGIN();

  set_global_address();

  simple_udp_register(&unicast_connection, UDP_PORT,
                      NULL, UDP_PORT, receiver);


  etimer_set(&energest_timer, CLOCK_SECOND * 20);
  etimer_set(&periodic_timer, SEND_INTERVAL);

  while(1) {

    PROCESS_YIELD();
    if (ev == PROCESS_EVENT_TIMER) {
      if (data == &energest_timer) {
        energest_flush();
        printf("\nEnergest:\n");
        printf(" CPU          %4lus LPM      %4lus DEEP LPM %4lus  Total time %lus\n",
              to_seconds(energest_type_time(ENERGEST_TYPE_CPU)),
              to_seconds(energest_type_time(ENERGEST_TYPE_LPM)),
              to_seconds(energest_type_time(ENERGEST_TYPE_DEEP_LPM)),
              to_seconds(ENERGEST_GET_TOTAL_TIME()));
        printf(" Radio LISTEN %4lus TRANSMIT %4lus OFF      %4lus\n",
              to_seconds(energest_type_time(ENERGEST_TYPE_LISTEN)),
              to_seconds(energest_type_time(ENERGEST_TYPE_TRANSMIT)),
              to_seconds(ENERGEST_GET_TOTAL_TIME()
                          - energest_type_time(ENERGEST_TYPE_TRANSMIT)
                          - energest_type_time(ENERGEST_TYPE_LISTEN)));
        etimer_reset(&energest_timer);
      }
      if (data == &periodic_timer) {
        default_prefix = uip_ds6_default_prefix();
        uip_ip6addr_copy(&addr, default_prefix);

        addr.u16[4] = UIP_HTONS(0x0201);
        addr.u16[5] = UIP_HTONS(0x0001);
        addr.u16[6] = UIP_HTONS(0x0001);
        addr.u16[7] = UIP_HTONS(0x0001);
        {
          static unsigned int message_number;
          char buf[20];

          printf("Sending unicast to ");
          uip_debug_ipaddr_print(&addr);
          printf("\n");
          sprintf(buf, "Message %d", message_number);
          message_number++;
          simple_udp_sendto(&unicast_connection, buf, strlen(buf) + 1, &addr);
        }
        etimer_reset(&periodic_timer);
      }
    }
    }





  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
