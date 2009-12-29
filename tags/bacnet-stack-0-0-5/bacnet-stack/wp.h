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
#ifndef WRITEPROPERTY_H
#define WRITEPROPERTY_H

#include <stdint.h>
#include <stdbool.h>
#include "bacdcode.h"

typedef struct BACnet_Write_Property_Value
{
  uint8_t tag;
  union
  {
    // NULL - not needed as it is encoded in the tag alone
    bool Boolean;
    unsigned Unsigned_Int;
    int Signed_Int;
    float Real;
    // Note: if you choose to enable the writing of certain types
    // you can uncomment the ones you need.  Good Luck!
    //double Double;
    //uint8_t Octet_String[20];
    //char Character_String[20];
    //BACNET_BIT_STRING Bit_String
    int Enumerated;
    BACNET_DATE Date;
    BACNET_TIME Time;
    struct
    {
      uint16_t type;
      uint32_t instance;
    } Object_ID;
  } type;
} BACNET_WRITE_PROPERTY_VALUE;

typedef struct BACnet_Write_Property_Data
{
  BACNET_OBJECT_TYPE object_type;
  uint32_t object_instance;
  BACNET_PROPERTY_ID object_property;
  int32_t array_index; // use BACNET_ARRAY_ALL when not setting
  BACNET_WRITE_PROPERTY_VALUE value;
  uint8_t priority; // use 0 if not setting the priority
} BACNET_WRITE_PROPERTY_DATA;

// encode service
int wp_encode_apdu(
  uint8_t *apdu, 
  uint8_t invoke_id,
  BACNET_WRITE_PROPERTY_DATA *data);

// decode the service request only
int wp_decode_service_request(
  uint8_t *apdu,
  unsigned apdu_len,
  BACNET_WRITE_PROPERTY_DATA *data);
  
int wp_decode_apdu(
  uint8_t *apdu,
  unsigned apdu_len,
  uint8_t *invoke_id,
  BACNET_WRITE_PROPERTY_DATA *data);

#ifdef TEST
#include "ctest.h"

void test_ReadProperty(Test * pTest);
void test_ReadPropertyAck(Test * pTest);
#endif

#endif
