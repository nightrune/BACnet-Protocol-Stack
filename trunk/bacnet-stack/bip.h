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
 Boston, MA  02111-1307, USA.

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
#ifndef BIP_H
#define BIP_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "bacdef.h"
#include "net.h"

/* specific defines for Ethernet */
#define MAX_HEADER (1 + 1 + 2)
#define MAX_MPDU (MAX_HEADER+MAX_PDU)

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

/* note: define init and cleanup in your ports section */
    bool bip_init(void);

/* normal functions... */
    void bip_cleanup(void);
    void bip_set_socket(int sock_fd);
    bool bip_valid(void);
    void bip_get_broadcast_address(BACNET_ADDRESS * dest);      /* destination address */
    void bip_get_my_address(BACNET_ADDRESS * my_address);

/* function to send a packet out the BACnet/IP socket */
/* returns zero on success, non-zero on failure */
    int bip_send_pdu(BACNET_ADDRESS * dest,     /* destination address */
        uint8_t * pdu,          /* any data to be sent - may be null */
        unsigned pdu_len);      /* number of bytes of data */

/* receives a BACnet/IP packet */
/* returns the number of octets in the PDU, or zero on failure */
    uint16_t bip_receive(BACNET_ADDRESS * src,  /* source address */
        uint8_t * pdu,          /* PDU data */
        uint16_t max_pdu,       /* amount of space available in the PDU  */
        unsigned timeout);      /* milliseconds to wait for a packet */

    void bip_set_address(uint8_t octet1, uint8_t octet2,
        uint8_t octet3, uint8_t octet4);
    void bip_set_broadcast_address(uint8_t octet1, uint8_t octet2,
        uint8_t octet3, uint8_t octet4);

/* use host byte order for setting */
    void bip_set_port(uint16_t port);
/* returns host byte order */
    uint16_t bip_get_port(void);

/* use network byte order for setting */
    void bip_set_addr(uint32_t net_address);
/* returns host byte order */
    uint32_t bip_get_addr(void);

/* use network byte order for setting */
    void bip_set_broadcast_addr(uint32_t net_address);
/* returns host byte order */
    uint32_t bip_get_broadcast_addr(void);

    void bip_set_interface(char *ifname);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif
