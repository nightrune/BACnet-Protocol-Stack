/*####COPYRIGHTBEGIN####
 -------------------------------------------
 Copyright (C) 2005 Steve Karg

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to:
 The Free Software Foundation, Inc.
 59 Temple Place - Suite 330
 Boston, MA  02111-1307
 USA.

 As a special exception, if other files instantiate templates or
 use macros or inline functions from this file, or you compile
 this file and link it with other works to produce a work based
 on this file, this file does not by itself cause the resulting
 work to be covered by the GNU General Public License. However
 the source code for this file must still be made available in
 accordance with section (3) of the GNU General Public License.

 This exception does not invalidate any other reasons why a work
 based on this file might be covered by the GNU General Public
 License.
 -------------------------------------------
####COPYRIGHTEND####*/
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "bacdef.h"
#include "mstp.h"
#include "dlmstp.h"

void dlmstp_init(void)
{
  
}

void dlmstp_cleanup(void)
{
  
}

/* returns number of bytes sent on success, negative on failure */
int dlmstp_send_pdu(
  BACNET_ADDRESS *dest,  // destination address
  uint8_t *pdu, // any data to be sent - may be null
  unsigned pdu_len) // number of bytes of data
{

  (void)dest;
  (void)pdu;
  (void)pdu_len;

  return 0;
}

// returns the number of octets in the PDU, or zero on failure
uint16_t dlmstp_receive(
  BACNET_ADDRESS *src,  // source address
  uint8_t *pdu, // PDU data
  uint16_t max_pdu,  // amount of space available in the PDU 
  unsigned timeout) // milliseconds to wait for a packet
{
  (void)src;
  (void)pdu;
  (void)max_pdu;
  (void)timeout;
  
  return 0;
}

void dlmstp_set_my_address(BACNET_ADDRESS *my_address)
{
  return;
}

void dlmstp_get_my_address(BACNET_ADDRESS *my_address)
{
  return;
}

void dlmstp_get_broadcast_address(
  BACNET_ADDRESS *dest)  // destination address
{
  return;
}