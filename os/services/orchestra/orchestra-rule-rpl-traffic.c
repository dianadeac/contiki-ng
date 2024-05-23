/*
 * Copyright (c) 2015, Swedish Institute of Computer Science.
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
 */
/**
 * \file
 *         Orchestra: a slotframe  just for RPL traffic
 *
 *
 */

#include "contiki.h"
#include "orchestra.h"

#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_DBG

#include "net/ipv6/uip-icmp6.h"
#include "net/routing/rpl-classic/rpl-private.h"
#include "net/routing/rpl-classic/rpl-dag-root.h"

static uint16_t slotframe_handle = 0;

/*---------------------------------------------------------------------------*/
static int
is_RPL_traffic()
{

  if (packetbuf_attr(PACKETBUF_ATTR_NETWORK_ID) == UIP_PROTO_ICMP6 &&
      (packetbuf_attr(PACKETBUF_ATTR_CHANNEL) == (ICMP6_RPL << 8 | RPL_CODE_DIO) 
        || packetbuf_attr(PACKETBUF_ATTR_CHANNEL) == (ICMP6_RPL << 8 | RPL_CODE_DIS) 
        ))
  {
    return 1;
  }
  return 0;
}

/*---------------------------------------------------------------------------*/
static int
select_packet(uint16_t *slotframe, uint16_t *timeslot, uint16_t *channel_offset)
{
  if (is_RPL_traffic())
  {
    /* This is the slotframe for RPL traffic so we direct on this slotframe only the RPL traffic */
    if (slotframe != NULL)
    {
      *slotframe = slotframe_handle;
    }
    if (timeslot != NULL)
    {
      *timeslot = 0;
    }
    printf("RPL RULE SELECT PACKET\n");
   

    return 1;
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
static void
init(uint16_t sf_handle)
{
  slotframe_handle = sf_handle;

  struct tsch_slotframe *sf_common = tsch_schedule_add_slotframe(slotframe_handle, ORCHESTRA_RPL_PERIOD);
  tsch_schedule_add_link(sf_common,
                         LINK_OPTION_RX | LINK_OPTION_TX | LINK_OPTION_SHARED,
                         LINK_TYPE_NORMAL, &tsch_broadcast_address,
                         0, 1, 1);
}
/*---------------------------------------------------------------------------*/
struct orchestra_rule rpl_traffic = {
    init,
    NULL,
    select_packet,
    NULL,
    NULL,
    NULL,
    NULL,
    "rpl traffic",
    ORCHESTRA_RPL_PERIOD,
};
