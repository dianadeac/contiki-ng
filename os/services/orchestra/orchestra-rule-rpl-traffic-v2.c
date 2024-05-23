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
uint16_t channel_offset = 0;

static struct tsch_slotframe *sf_rpl;
static struct tsch_link *rpl_link;

/*---------------------------------------------------------------------------*/
static uint16_t
get_node_timeslot(const linkaddr_t *addr)
{
  if (addr != NULL && ORCHESTRA_UNICAST_PERIOD > 0)
  {
    return ORCHESTRA_LINKADDR_HASH(addr) % ORCHESTRA_UNICAST_PERIOD;
  }
  else
  {
    return 0xffff;
  }
}
/*---------------------------------------------------------------------------*/
// static uint16_t
// get_node_channel_offset(const linkaddr_t *addr)
// {
//   if(addr != NULL && ORCHESTRA_UNICAST_MAX_CHANNEL_OFFSET >= ORCHESTRA_UNICAST_MIN_CHANNEL_OFFSET) {
//     return ORCHESTRA_LINKADDR_HASH(addr) % (ORCHESTRA_UNICAST_MAX_CHANNEL_OFFSET - ORCHESTRA_UNICAST_MIN_CHANNEL_OFFSET + 1)
//         + ORCHESTRA_UNICAST_MIN_CHANNEL_OFFSET;
//   } else {
//     return 0xffff;
//   }
// }
/*---------------------------------------------------------------------------*/
static int
is_RPL_traffic()
{

  if (packetbuf_attr(PACKETBUF_ATTR_NETWORK_ID) == UIP_PROTO_ICMP6 &&
      (packetbuf_attr(PACKETBUF_ATTR_CHANNEL) == (ICMP6_RPL << 8 | RPL_CODE_DIO) || packetbuf_attr(PACKETBUF_ATTR_CHANNEL) == (ICMP6_RPL << 8 | RPL_CODE_DIS) || packetbuf_attr(PACKETBUF_ATTR_CHANNEL) == (ICMP6_RPL << 8 | RPL_CODE_DAO_ACK) || packetbuf_attr(PACKETBUF_ATTR_CHANNEL) == (ICMP6_RPL << 8 | RPL_CODE_DAO)))
  
  {
    return 1;
  }
  return 0;
}
/*---------------------------------------------------------------------------*/

static void
add_uc_link(const linkaddr_t *linkaddr)
{
  if(linkaddr != NULL) {
    uint16_t timeslot = get_node_timeslot(linkaddr);

    /* Add a Tx link to the neighbor; do not replace any existing links
     * at that cell. The channel offset here does not matter:
     * select_packet() always sets the right channel offset per packet. */
    tsch_schedule_add_link(sf_rpl,
        LINK_OPTION_SHARED | LINK_OPTION_RX,
        LINK_TYPE_NORMAL, &tsch_broadcast_address,
        timeslot, channel_offset, 0);
  }
}
/*---------------------------------------------------------------------------*/

static void
remove_uc_link(const linkaddr_t *linkaddr)
{
  if(linkaddr != NULL) {
    uint16_t timeslot = get_node_timeslot(linkaddr);
    tsch_schedule_remove_link_by_offsets(sf_rpl, timeslot, channel_offset);
    tsch_queue_free_packets_to(linkaddr);
  }
}
/*---------------------------------------------------------------------------*/
static void
neighbor_updated(const linkaddr_t *linkaddr, uint8_t is_added)
{
  if(is_added) {
    add_uc_link(linkaddr);
  } else {
    remove_uc_link(linkaddr);
  }
}
/*---------------------------------------------------------------------------*/
static int
select_packet(uint16_t *slotframe, uint16_t *timeslot, uint16_t *channel_offset)
{
  /* Select data packets we have a unicast link to */
  linkaddr_t *local_addr = &linkaddr_node_addr;
  if (is_RPL_traffic())
  {
    if (slotframe != NULL)
    {
      *slotframe = slotframe_handle;
    }
    if (timeslot != NULL)
    {
      *timeslot = get_node_timeslot(local_addr);
    }
    // /* set per-packet channel offset */
    // if(channel_offset != NULL) {
    //   *channel_offset = get_node_channel_offset(dest);
    // }
    printf("RPL RULE 2 SELECT PACKET\n");
    return 1;
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
static void
new_time_source(const struct tsch_neighbor *old, const struct tsch_neighbor *new)
{
  uint16_t old_ts = 0xffff;
  uint16_t new_ts = 0xffff;
  // uint16_t old_channel_offset = 0xffff;
  // uint16_t new_channel_offset = 0xffff;

  if (old != NULL)
  {
    const linkaddr_t *addr = tsch_queue_get_nbr_address(old);
    old_ts = get_node_timeslot(addr);
    // old_channel_offset = get_node_channel_offset(addr);
  }

  if (new != NULL)
  {
    const linkaddr_t *addr = tsch_queue_get_nbr_address(new);
    new_ts = get_node_timeslot(addr);
    // new_channel_offset = get_node_channel_offset(addr);
  }

  if (new_ts == old_ts)
  {
    return;
  }

  if (rpl_link != NULL)
  {
    /* Stop listening to the old parent*/
    tsch_schedule_remove_link(sf_rpl, rpl_link);
    rpl_link = NULL;
  }
  if (new_ts != 0xffff)
  {
    /* Listen to messages from parent */
    rpl_link = tsch_schedule_add_link(sf_rpl, LINK_OPTION_RX, LINK_TYPE_NORMAL,
                                      &tsch_broadcast_address, new_ts, 0, 0);
  }
}
/*---------------------------------------------------------------------------*/
static void
init(uint16_t sf_handle)
{

  uint16_t tx_timeslot;
  linkaddr_t *local_addr = &linkaddr_node_addr;

  struct tsch_link *link;

  slotframe_handle = sf_handle;
  /* Slotframe for RPL transmissions */
  sf_rpl = tsch_schedule_add_slotframe(slotframe_handle, ORCHESTRA_RPL_PERIOD);
  /* When divices wake up they allocate a Tx (for DIO transmission)*/
  tx_timeslot = get_node_timeslot(local_addr);
  link = tsch_schedule_add_link(sf_rpl,
                                LINK_OPTION_TX,
                                LINK_TYPE_NORMAL, &tsch_broadcast_address,
                                tx_timeslot, channel_offset, 1);
  if (link == NULL)
  {
    printf("Adding a RPL link failed \n");
  }
  else
  {

    printf("Added RPL link -> cell for Tx at: timeslot %u and channel offset %u \n", tx_timeslot, channel_offset);
  }
}

/*---------------------------------------------------------------------------*/
struct orchestra_rule rpl_traffic_v2 = {
    init,
    new_time_source,
    select_packet,
    NULL,
    NULL,
    neighbor_updated,
    NULL,
    "rpl traffic v2",
    ORCHESTRA_RPL_PERIOD,
};
