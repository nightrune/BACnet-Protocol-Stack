/*####COPYRIGHTBEGIN####
 -------------------------------------------
 Copyright (C) 2004 Steve Karg

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

#include <string.h>
#include <assert.h>

#include "bacdcode.h"
#include "bacenum.h"
#include "bits.h"
#include "bigend.h"

#ifndef MAX_APDU
#define MAX_APDU 480
#endif

// NOTE: byte order plays a role in decoding multibyte values
// http://www.unixpapa.com/incnote/byteorder.html

/* max-segments-accepted
   B'000'      Unspecified number of segments accepted.
   B'001'      2 segments accepted.
   B'010'      4 segments accepted.
   B'011'      8 segments accepted.
   B'100'      16 segments accepted.
   B'101'      32 segments accepted.
   B'110'      64 segments accepted.
   B'111'      Greater than 64 segments accepted.
*/

/* max-APDU-length-accepted
   B'0000'  Up to MinimumMessageSize (50 octets)
   B'0001'  Up to 128 octets
   B'0010'  Up to 206 octets (fits in a LonTalk frame)
   B'0011'  Up to 480 octets (fits in an ARCNET frame)
   B'0100'  Up to 1024 octets
   B'0101'  Up to 1476 octets (fits in an ISO 8802-3 frame)
   B'0110'  reserved by ASHRAE
   B'0111'  reserved by ASHRAE
   B'1000'  reserved by ASHRAE
   B'1001'  reserved by ASHRAE
   B'1010'  reserved by ASHRAE
   B'1011'  reserved by ASHRAE
   B'1100'  reserved by ASHRAE
   B'1101'  reserved by ASHRAE
   B'1110'  reserved by ASHRAE
   B'1111'  reserved by ASHRAE
*/
// from clause 20.1.2.4 max-segments-accepted
// and clause 20.1.2.5 max-APDU-length-accepted
// returns the encoded octet
uint8_t encode_max_segs_max_apdu(int max_segs, int max_apdu)
{
    uint8_t octet = 0;

    if (max_segs < 2)
        octet = 0;
    else if (max_segs < 4)
        octet = 0x10;
    else if (max_segs < 8)
        octet = 0x20;
    else if (max_segs < 16)
        octet = 0x30;
    else if (max_segs < 32)
        octet = 0x40;
    else if (max_segs < 64)
        octet = 0x50;
    else if (max_segs == 64)
        octet = 0x60;
    else
        octet = 0x70;

    // max_apdu must be 50 octets minimum
    assert(max_apdu >= 50);
    if (max_apdu == 50)
       octet |= 0x00;
    else if (max_apdu <= 128)
       octet |= 0x01;
    //fits in a LonTalk frame
    else if (max_apdu <= 206)
       octet |= 0x02;
    //fits in an ARCNET or MS/TP frame
    else if (max_apdu <= 480)
       octet |= 0x03;
    else if (max_apdu <= 1024)
       octet |= 0x04;
    // fits in an ISO 8802-3 frame
    else if (max_apdu <= 1476)
       octet |= 0x05;

    return octet;
}


// from clause 20.2.1 General Rules for Encoding BACnet Tags
// returns the number of apdu bytes consumed
int encode_tag(uint8_t * apdu, uint8_t tag_number, bool context_specific,
    uint32_t len_value_type)
{
    int len = 1;                // return value
    union {
        uint8_t byte[2];
        uint16_t value;
    } short_data = { {
    0}};
    union {
        uint8_t byte[4];
        uint32_t value;
    } long_data = { {
    0}};

    apdu[0] = 0;
    if (context_specific)
        apdu[0] = BIT3;

    // additional tag byte after this byte 
    // for extended tag byte
    if (tag_number <= 14)
        apdu[0] |= (tag_number << 4);
    else {
        apdu[0] |= 0xF0;
        apdu[1] = tag_number;
        len++;
    }

    // NOTE: additional len byte(s) after extended tag byte
    // if larger than 4
    if (len_value_type <= 4)
        apdu[0] |= len_value_type;
    else {
        apdu[0] |= 5;
        if (len_value_type <= 253) {
            apdu[len] = len_value_type;
            len++;
        } else if (len_value_type <= 65535) {
            apdu[len] = 254;
            len++;
            short_data.value = len_value_type;
            if (big_endian()) {
                apdu[len + 0] = short_data.byte[0];
                apdu[len + 1] = short_data.byte[1];
            } else {
                apdu[len + 0] = short_data.byte[1];
                apdu[len + 1] = short_data.byte[0];
            }
            len += 2;
        } else {
            apdu[len] = 255;
            len++;
            long_data.value = len_value_type;
            if (big_endian()) {
                apdu[len + 0] = long_data.byte[0];
                apdu[len + 1] = long_data.byte[1];
                apdu[len + 2] = long_data.byte[2];
                apdu[len + 3] = long_data.byte[3];
            } else {
                apdu[len + 0] = long_data.byte[3];
                apdu[len + 1] = long_data.byte[2];
                apdu[len + 2] = long_data.byte[1];
                apdu[len + 3] = long_data.byte[0];
            }
            len += 4;
        }
    }

    return len;
}

// from clause 20.2.1.3.2 Constructed Data
// returns the number of apdu bytes consumed
int encode_opening_tag(uint8_t * apdu, uint8_t tag_number)
{
    int len = 1;

    apdu[0] = BIT3;
    // additional tag byte after this byte for extended tag byte
    if (tag_number <= 14)
        apdu[0] |= (tag_number << 4);
    else {
        apdu[0] |= 0xF0;
        apdu[1] = tag_number;
        len++;
    }

    // type indicates opening tag
    apdu[0] |= 6;

    return len;
}

// from clause 20.2.1.3.2 Constructed Data
// returns the number of apdu bytes consumed
int encode_closing_tag(uint8_t * apdu, uint8_t tag_number)
{
    int len = 1;

    apdu[0] = BIT3;
    // additional tag byte after this byte for extended tag byte
    if (tag_number <= 14)
        apdu[0] |= (tag_number << 4);
    else {
        apdu[0] |= 0xF0;
        apdu[1] = tag_number;
        len++;
    }

    // type indicates closing tag
    apdu[0] |= 7;

    return len;
}

// from clause 20.2.1.3.2 Constructed Data
// returns true if extended tag numbering is used
static bool decode_is_extended_tag_number(uint8_t * apdu)
{
    return ((apdu[0] & 0xF0) == 0xF0);
}

// from clause 20.2.1.3.2 Constructed Data
// returns true if the extended value is used
static bool decode_is_extended_value(uint8_t * apdu)
{
    return ((apdu[0] & 0x07) == 5);
}

// from clause 20.2.1.3.2 Constructed Data
// returns the number of apdu bytes consumed
bool decode_is_context_specific(uint8_t * apdu)
{
    return (apdu[0] & BIT3);
}

// from clause 20.2.1.3.2 Constructed Data
// returns the number of apdu bytes consumed
static bool decode_is_opening_tag(uint8_t * apdu)
{
    return ((apdu[0] & 0x07) == 6);
}

// from clause 20.2.1.3.2 Constructed Data
// returns the number of apdu bytes consumed
static bool decode_is_closing_tag(uint8_t * apdu)
{
    return ((apdu[0] & 0x07) == 7);
}

int decode_tag_number(uint8_t * apdu,
    uint8_t * tag_number)
{
    int len = 1; // return value

    // decode the tag number first
    if (decode_is_extended_tag_number(&apdu[0])) {
        // extended tag
        if (tag_number)
            *tag_number = apdu[1];
        len++;
    } else {
        if (tag_number)
            *tag_number = (apdu[0] >> 4);
    }

    return len;
}

// from clause 20.2.1.3.2 Constructed Data
// returns the number of apdu bytes consumed
// from clause 20.2.1.3.2 Constructed Data
// returns the number of apdu bytes consumed
int decode_tag_number_and_value(uint8_t * apdu,
    uint8_t * tag_number, uint32_t * value)
{
    int len = 1;
    union {
        uint8_t byte[2];
        uint16_t value;
    } short_data = { {
    0}};
    union {
        uint8_t byte[4];
        uint32_t value;
    } long_data = { {
    0}};

    len = decode_tag_number(&apdu[0],tag_number);
    // decode the value
    if (decode_is_extended_value(&apdu[0])) {
        // tagged as uint32_t
        if (apdu[len] == 255) {
            len++;
            if (big_endian()) {
                long_data.byte[0] = apdu[len + 0];
                long_data.byte[1] = apdu[len + 1];
                long_data.byte[2] = apdu[len + 2];
                long_data.byte[3] = apdu[len + 3];
            } else {
                long_data.byte[3] = apdu[len + 0];
                long_data.byte[2] = apdu[len + 1];
                long_data.byte[1] = apdu[len + 2];
                long_data.byte[0] = apdu[len + 3];
            }
            if (value)
                *value = long_data.value;
            len += 4;

        }
        // tagged as uint16_t
        else if (apdu[len] == 254) {
            len++;
            if (big_endian()) {
                short_data.byte[0] = apdu[len + 0];
                short_data.byte[1] = apdu[len + 1];
            } else {
                short_data.byte[1] = apdu[len + 0];
                short_data.byte[0] = apdu[len + 1];
            }
            if (value)
                *value = short_data.value;
            len += 2;

        }
        // no tag - must be uint8_t 
        else {
            if (value)
                *value = apdu[len];
            len++;
        }
    } else if (decode_is_opening_tag(&apdu[0]) && value)
        *value = 0;
    // closing tag
    else if (decode_is_closing_tag(&apdu[0]) && value)
        *value = 0;
    // small value 
    else if (value)
        *value = apdu[0] & 0x07;

    return len;
}

// from clause 20.2.1.3.2 Constructed Data
// returns true if the tag is context specific and matches
bool decode_is_context_tag(uint8_t * apdu, uint8_t tag_number)
{
    uint8_t my_tag_number = 0;
    bool context_specific = false;

    context_specific = decode_is_context_specific(apdu);
    decode_tag_number(apdu,&my_tag_number);

    return (context_specific && (my_tag_number == tag_number));
}

// from clause 20.2.1.3.2 Constructed Data
// returns the true if the tag is an opening tag and the tag number matches
bool decode_is_opening_tag_number(uint8_t * apdu, uint8_t tag_number)
{
    uint8_t my_tag_number = 0;
    bool opening_tag = false;

    opening_tag = decode_is_opening_tag(apdu);
    decode_tag_number(apdu,&my_tag_number);

    return (opening_tag && (my_tag_number == tag_number));
}

// from clause 20.2.1.3.2 Constructed Data
// returns the true if the tag is a closing tag and the tag number matches
bool decode_is_closing_tag_number(uint8_t * apdu, uint8_t tag_number)
{
    uint8_t my_tag_number = 0;
    bool closing_tag = false;

    closing_tag = decode_is_closing_tag(apdu);
    decode_tag_number(apdu,&my_tag_number);

    return (closing_tag && (my_tag_number == tag_number));
}

// from clause 20.2.6 Encoding of a Real Number Value
// returns the number of apdu bytes consumed
int decode_real(uint8_t * apdu, float *real_value)
{
    union {
        uint8_t byte[4];
        float real_value;
    } my_data;

    if (big_endian()) {
        my_data.byte[0] = apdu[0];
        my_data.byte[1] = apdu[1];
        my_data.byte[2] = apdu[2];
        my_data.byte[3] = apdu[3];
    } else {
        my_data.byte[0] = apdu[3];
        my_data.byte[1] = apdu[2];
        my_data.byte[2] = apdu[1];
        my_data.byte[3] = apdu[0];
    }

    *real_value = my_data.real_value;

    return 4;
}

// from clause 20.2.6 Encoding of a Real Number Value
// returns the number of apdu bytes consumed
int encode_bacnet_real(float value, uint8_t * apdu)
{
    union {
        uint8_t byte[4];
        float real_value;
    } my_data;

    my_data.real_value = value;

    if (big_endian()) {
        apdu[0] = my_data.byte[0];
        apdu[1] = my_data.byte[1];
        apdu[2] = my_data.byte[2];
        apdu[3] = my_data.byte[3];
    } else {
        apdu[0] = my_data.byte[3];
        apdu[1] = my_data.byte[2];
        apdu[2] = my_data.byte[1];
        apdu[3] = my_data.byte[0];
    }

    return 4;
}

// from clause 20.2.6 Encoding of a Real Number Value
// and 20.2.1 General Rules for Encoding BACnet Tags
// returns the number of apdu bytes consumed
int encode_tagged_real(uint8_t * apdu, float value)
{
    int len = 0;

    // assumes that the tag only consumes 1 octet
    len = encode_bacnet_real(value, &apdu[1]);
    len += encode_tag(&apdu[0], BACNET_APPLICATION_TAG_REAL, false, len);

    return len;
}

// from clause 20.2.14 Encoding of an Object Identifier Value
// returns the number of apdu bytes consumed
int decode_object_id(uint8_t * apdu, int *object_type, int *instance)
{
    union {
        uint8_t byte[4];
        uint32_t value;
    } my_data;

    if (big_endian()) {
        my_data.byte[0] = apdu[0];
        my_data.byte[1] = apdu[1];
        my_data.byte[2] = apdu[2];
        my_data.byte[3] = apdu[3];
    } else {
        my_data.byte[0] = apdu[3];
        my_data.byte[1] = apdu[2];
        my_data.byte[2] = apdu[1];
        my_data.byte[3] = apdu[0];
    }

    *object_type = ((my_data.value >> 22) & 0x3FF);
    *instance = (my_data.value & 0x3FFFFF);

    return 4;
}

// from clause 20.2.14 Encoding of an Object Identifier Value
// returns the number of apdu bytes consumed
int encode_bacnet_object_id(uint8_t * apdu, int object_type, int instance)
{
    union {
        uint8_t byte[4];
        uint32_t value;
    } my_data;

    my_data.value = ((object_type & 0x3FF) << 22) | (instance & 0x3FFFFF);

    if (big_endian()) {
        apdu[0] = my_data.byte[0];
        apdu[1] = my_data.byte[1];
        apdu[2] = my_data.byte[2];
        apdu[3] = my_data.byte[3];
    } else {
        apdu[0] = my_data.byte[3];
        apdu[1] = my_data.byte[2];
        apdu[2] = my_data.byte[1];
        apdu[3] = my_data.byte[0];
    }

    return 4;
}

// from clause 20.2.14 Encoding of an Object Identifier Value
// and 20.2.1 General Rules for Encoding BACnet Tags
// returns the number of apdu bytes consumed
int encode_context_object_id(uint8_t * apdu, int tag_number,
    int object_type, int instance)
{
    int len = 0;

    // assumes that the tag only consumes 1 octet
    len = encode_bacnet_object_id(&apdu[1], object_type, instance);
    len += encode_tag(&apdu[0], tag_number, true, len);

    return len;
}

// from clause 20.2.14 Encoding of an Object Identifier Value
// and 20.2.1 General Rules for Encoding BACnet Tags
// returns the number of apdu bytes consumed
int encode_tagged_object_id(uint8_t * apdu, int object_type, int instance)
{
    int len = 0;

    // assumes that the tag only consumes 1 octet
    len = encode_bacnet_object_id(&apdu[1], object_type, instance);
    len += encode_tag(&apdu[0], BACNET_APPLICATION_TAG_OBJECT_ID,
        false, len);

    return len;
}

// from clause 20.2.9 Encoding of a Character String Value
// returns the number of apdu bytes consumed
int encode_bacnet_string(uint8_t * apdu, const char *char_string, int len)
{
    int i;

    // limit - 6 octets is the most our tag and type could be
    if (len > (MAX_APDU - 6))
        len = MAX_APDU - 6;
    for (i = 0; i < len; i++) {
        apdu[1 + i] = char_string[i];
    }
    apdu[0] = CHARACTER_ANSI;
    len++;

    return len;
}

// from clause 20.2.9 Encoding of a Character String Value
// returns the number of apdu bytes consumed
int encode_bacnet_character_string(uint8_t * apdu, const char *char_string)
{
    int len;

    len = strlen(char_string);
    len = encode_bacnet_string(&apdu[0], char_string, len);

    return len;
}

// from clause 20.2.9 Encoding of a Character String Value
// and 20.2.1 General Rules for Encoding BACnet Tags
// returns the number of apdu bytes consumed
int encode_tagged_character_string(uint8_t * apdu, const char *char_string)
{
    int len = 0;
    int string_len = 0;

    // find the size of the apdu first - not necessarily effecient
    // but reuses existing functions
    string_len = encode_bacnet_character_string(&apdu[1], char_string);
    len = encode_tag(&apdu[0], BACNET_APPLICATION_TAG_CHARACTER_STRING,
        false, string_len);
    assert((len + string_len) < MAX_APDU);
    len += encode_bacnet_character_string(&apdu[len], char_string);

    return len;
}

// from clause 20.2.9 Encoding of a Character String Value
// and 20.2.1 General Rules for Encoding BACnet Tags
// returns the number of apdu bytes consumed
int decode_character_string(uint8_t * apdu, char *char_string)
{
    int len = 0, i = 0;
    uint32_t len_value = 0;
    uint8_t tag_number = 0;

    len = decode_tag_number_and_value(&apdu[0], &tag_number, &len_value);
    if (len_value) {
        // decode ANSI X3.4
        if (apdu[len] == 0) {
            len++;
            len_value--;
            for (i = 0; i < len_value; i++) {
                char_string[i] = apdu[len + i];
            }
            // terminate the c string
            char_string[i] = 0;
            len += len_value;
        }
    }

    return len;
}

// from clause 20.2.4 Encoding of an Unsigned Integer Value
// and 20.2.1 General Rules for Encoding BACnet Tags
// returns the number of apdu bytes consumed
int encode_bacnet_unsigned(uint8_t * apdu, unsigned int value)
{
    int len = 0;                // return value

    if (value < 0x100) {
        apdu[0] = value;
        len = 1;
    } else if (value < 0x10000) {
        apdu[0] = value / 0x100;
        apdu[1] = value - (apdu[0] * 0x100);
        len = 2;
    } else if (value < 0x1000000) {
        apdu[0] = value / 0x10000;
        apdu[1] = (value - apdu[0] * 0x10000) / 0x100;
        apdu[2] = value - (apdu[0] * 0x10000) - (apdu[1] * 0x100);
        len = 3;
    } else {
        apdu[0] = value / 0x1000000;
        apdu[1] = (value - (apdu[0] * 0x1000000)) / 0x10000;
        apdu[2] =
            (value - (apdu[0] * 0x1000000) - (apdu[1] * 0x10000)) / 0x100;
        apdu[3] =
            value - (apdu[0] * 0x1000000) - (apdu[1] * 0x10000) -
            (apdu[2] * 0x100);
        len = 4;
    }

    return len;
}

// from clause 20.2.4 Encoding of an Unsigned Integer Value
// and 20.2.1 General Rules for Encoding BACnet Tags
// returns the number of apdu bytes consumed
int encode_context_unsigned(uint8_t * apdu, int tag_number, int value)
{
    int len = 0;

    len = encode_bacnet_unsigned(&apdu[1], value);
    len += encode_tag(&apdu[0], tag_number, true, len);

    return len;
}

// from clause 20.2.4 Encoding of an Unsigned Integer Value
// and 20.2.1 General Rules for Encoding BACnet Tags
// returns the number of apdu bytes consumed
int encode_tagged_unsigned(uint8_t * apdu, unsigned int value)
{
    int len = 0;

    len = encode_bacnet_unsigned(&apdu[1], value);
    len += encode_tag(&apdu[0], BACNET_APPLICATION_TAG_UNSIGNED_INT,
        false, len);

    return len;
}

// from clause 20.2.4 Encoding of an Unsigned Integer Value
// and 20.2.1 General Rules for Encoding BACnet Tags
// returns the number of apdu bytes consumed
int decode_unsigned(uint8_t * apdu, int *value)
{
    int len = 0;                // return value
    uint32_t len_value = 0;
    uint8_t tag_number = 0;

    len = decode_tag_number_and_value(&apdu[0], &tag_number, &len_value);

    switch (len_value) {
    case 1:
        *value = apdu[len];
        break;
    case 2:
        *value = (apdu[len] * 0x100) + apdu[len + 1];
        break;
    case 3:
        *value = (apdu[len] * 0x10000) +
            (apdu[len + 1] * 0x100) + apdu[len + 2];
        break;
    case 4:
        *value = (apdu[len] * 0x1000000) +
            (apdu[len + 1] * 0x10000) + (apdu[len + 2] * 0x100) +
            apdu[len + 3];
        break;
    default:
        *value = 0;
        break;
    }
    len += len_value;

    return len;
}

// from clause 20.2.11 Encoding of an Enumerated Value
// and 20.2.1 General Rules for Encoding BACnet Tags
// returns the number of apdu bytes consumed
int decode_enumerated(uint8_t * apdu, int *value)
{
    return decode_unsigned(apdu, value);
}

// from clause 20.2.11 Encoding of an Enumerated Value
// and 20.2.1 General Rules for Encoding BACnet Tags
// returns the number of apdu bytes consumed
int encode_bacnet_enumerated(uint8_t * apdu, int value)
{
    return encode_bacnet_unsigned(apdu, value);
}

// from clause 20.2.11 Encoding of an Enumerated Value
// and 20.2.1 General Rules for Encoding BACnet Tags
// returns the number of apdu bytes consumed
int encode_tagged_enumerated(uint8_t * apdu, int value)
{
    int len = 0;                // return value

    // assumes that the tag only consumes 1 octet
    len = encode_bacnet_enumerated(&apdu[1], value);
    len += encode_tag(&apdu[0], BACNET_APPLICATION_TAG_ENUMERATED,
        false, len);

    return len;
}

// from clause 20.2.11 Encoding of an Enumerated Value
// and 20.2.1 General Rules for Encoding BACnet Tags
// returns the number of apdu bytes consumed
int encode_context_enumerated(uint8_t * apdu, int tag_number, int value)
{
    int len = 0;                // return value

    // assumes that the tag only consumes 1 octet
    len = encode_bacnet_enumerated(&apdu[1], value);
    len += encode_tag(&apdu[0], tag_number, true, len);

    return len;
}

// from clause 20.2.5 Encoding of a Signed Integer Value
// and 20.2.1 General Rules for Encoding BACnet Tags
// returns the number of apdu bytes consumed
int decode_signed(uint8_t * apdu, int *value)
{
    return decode_unsigned(apdu, value);
}

// from clause 20.2.5 Encoding of a Signed Integer Value
// and 20.2.1 General Rules for Encoding BACnet Tags
// returns the number of apdu bytes consumed
int encode_bacnet_signed(uint8_t * apdu, int value)
{
    return encode_bacnet_unsigned(apdu, value);
}

// from clause 20.2.5 Encoding of a Signed Integer Value
// and 20.2.1 General Rules for Encoding BACnet Tags
// returns the number of apdu bytes consumed
int encode_tagged_signed(uint8_t * apdu, int value)
{
    int len = 0;                // return value

    // assumes that the tag only consumes 1 octet
    len = encode_bacnet_signed(&apdu[1], value);
    len += encode_tag(&apdu[0], BACNET_APPLICATION_TAG_SIGNED_INT,
        false, len);

    return len;
}

// from clause 20.2.5 Encoding of a Signed Integer Value
// and 20.2.1 General Rules for Encoding BACnet Tags
// returns the number of apdu bytes consumed
int encode_context_signed(uint8_t * apdu, int tag_number, int value)
{
    int len = 0;                // return value

    // assumes that the tag only consumes 1 octet
    len = encode_bacnet_signed(&apdu[1], value);
    len += encode_tag(&apdu[0], tag_number, true, len);

    return len;
}

// from clause 20.2.13 Encoding of a Time Value
// and 20.2.1 General Rules for Encoding BACnet Tags
// returns the number of apdu bytes consumed
int encode_bacnet_time(uint8_t * apdu, int hour, int min, int sec,
    int hundredths)
{
    apdu[0] = hour;
    apdu[1] = min;
    apdu[2] = sec;
    apdu[3] = hundredths;

    return 4;
}

// from clause 20.2.13 Encoding of a Time Value
// and 20.2.1 General Rules for Encoding BACnet Tags
// returns the number of apdu bytes consumed
int encode_tagged_time(uint8_t * apdu, int hour, int min, int sec, int hundredths)
{
    int len = 0;

    // assumes that the tag only consumes 1 octet
    len = encode_bacnet_time(&apdu[1], hour, min, sec, hundredths);
    len += encode_tag(&apdu[0], BACNET_APPLICATION_TAG_TIME, false, len);

    return len;

}

// from clause 20.2.13 Encoding of a Time Value
// and 20.2.1 General Rules for Encoding BACnet Tags
// returns the number of apdu bytes consumed
int decode_bacnet_time(uint8_t * apdu, int *hour, int *min, int *sec,
    int *hundredths)
{
    *hour = apdu[0];
    *min = apdu[1];
    *sec = apdu[2];
    *hundredths = apdu[3];

    return 4;
}

// BACnet Date
// year = years since 1900
// month 1=Jan
// day = day of month
// wday 1=Monday...7=Sunday

// from clause 20.2.12 Encoding of a Date Value
// and 20.2.1 General Rules for Encoding BACnet Tags
// returns the number of apdu bytes consumed
int encode_bacnet_date(uint8_t * apdu, int year, int month, int day,
    int wday)
{
    apdu[0] = year;
    apdu[1] = month;
    apdu[2] = day;
    apdu[3] = wday;

    return 4;
}

// from clause 20.2.12 Encoding of a Date Value
// and 20.2.1 General Rules for Encoding BACnet Tags
// returns the number of apdu bytes consumed
int encode_tagged_date(uint8_t * apdu, int year, int month, int day, int wday)
{
    int len = 0;

    // assumes that the tag only consumes 1 octet
    len = encode_bacnet_date(&apdu[1], year, month, day, wday);
    len += encode_tag(&apdu[0], BACNET_APPLICATION_TAG_DATE, false, len);

    return len;

}

// from clause 20.2.12 Encoding of a Date Value
// and 20.2.1 General Rules for Encoding BACnet Tags
// returns the number of apdu bytes consumed
int decode_date(uint8_t * apdu, int *year, int *month, int *day, int *wday)
{
    *year = apdu[0];
    *month = apdu[1];
    *day = apdu[2];
    *wday = apdu[3];

    return 4;
}

/* end of decoding_encoding.c */
#ifdef TEST
#include <assert.h>
#include <string.h>
#include "ctest.h"

static int get_apdu_len(bool extended_tag, uint32_t value)
{
    int test_len = 1;

    if (extended_tag)
        test_len++;
    if (value <= 4)
        test_len += 0;          // do nothing...
    else if (value <= 253)
        test_len += 1;
    else if (value <= 65535)
        test_len += 3;
    else
        test_len += 5;

    return test_len;
}

void testBACDCodeTags(Test * pTest)
{
    uint8_t apdu[MAX_APDU] = { 0 };
    uint8_t tag_number = 0, test_tag_number = 0;
    int len = 0, test_len = 0;
    uint32_t value = 0, test_value = 0;

    for (tag_number = 0;; tag_number++) {
        len = encode_opening_tag(&apdu[0], tag_number);
        test_len =
            get_apdu_len(decode_is_extended_tag_number(&apdu[0]), 0);
        ct_test(pTest, len == test_len);
        len =
            decode_tag_number_and_value(&apdu[0], &test_tag_number,
            &value);
        ct_test(pTest, value == 0);
        ct_test(pTest, len == test_len);
        ct_test(pTest, tag_number == test_tag_number);
        ct_test(pTest, decode_is_opening_tag(&apdu[0]) == true);
        ct_test(pTest, decode_is_closing_tag(&apdu[0]) == false);
        len = encode_closing_tag(&apdu[0], tag_number);
        ct_test(pTest, len == test_len);
        len =
            decode_tag_number_and_value(&apdu[0], &test_tag_number,
            &value);
        ct_test(pTest, len == test_len);
        ct_test(pTest, value == 0);
        ct_test(pTest, tag_number == test_tag_number);
        ct_test(pTest, decode_is_opening_tag(&apdu[0]) == false);
        ct_test(pTest, decode_is_closing_tag(&apdu[0]) == true);
        // test the len-value-type portion 
        for (value = 1;; value = value << 1) {
            len = encode_tag(&apdu[0], tag_number, false, value);
            len = decode_tag_number_and_value(&apdu[0], &test_tag_number,
                &test_value);
            ct_test(pTest, tag_number == test_tag_number);
            ct_test(pTest, value == test_value);
            test_len =
                get_apdu_len(decode_is_extended_tag_number(&apdu[0]),
                value);
            ct_test(pTest, len == test_len);
            // stop at the the last value
            if (value & BIT31)
                break;
        }
        // stop after the last tag number
        if (tag_number == 255)
            break;
    }

    return;
}

void testBACDCodeReal(Test * pTest)
{
    uint8_t real_array[4] = { 0 };
    uint8_t encoded_array[4] = { 0 };
    float value = 42.123;
    float decoded_value = 0;
    uint8_t apdu[MAX_APDU] = { 0 };
    int len = 0, apdu_len = 0;
    uint8_t tag_number = 0;
    uint32_t long_value = 0;

    encode_bacnet_real(value, &real_array[0]);
    decode_real(&real_array[0], &decoded_value);
    ct_test(pTest, decoded_value == value);
    encode_bacnet_real(value, &encoded_array[0]);
    ct_test(pTest, memcmp(&real_array, &encoded_array,
            sizeof(real_array)) == 0);

    // a real will take up 4 octects plus a one octet tag 
    apdu_len = encode_tagged_real(&apdu[0], value);
    ct_test(pTest, apdu_len == 5);
    // len tells us how many octets were used for encoding the value
    len = decode_tag_number_and_value(&apdu[0], &tag_number, &long_value);
    ct_test(pTest, tag_number == BACNET_APPLICATION_TAG_REAL);
    ct_test(pTest, decode_is_context_specific(&apdu[0]) == false);
    ct_test(pTest, len == 1);
    ct_test(pTest, long_value == 4);
    decode_real(&apdu[len], &decoded_value);
    ct_test(pTest, decoded_value == value);

    return;
}

void testBACDCodeEnumerated(Test * pTest)
{
    uint8_t array[5] = { 0 };
    uint8_t encoded_array[5] = { 0 };
    unsigned int value = 1;
    unsigned int decoded_value = 0;
    int i = 0, apdu_len = 0;
    int len = 0;
    uint8_t apdu[MAX_APDU] = { 0 };
    uint8_t tag_number = 0;

    for (i = 0; i < 8; i++) {
        encode_tagged_enumerated(&array[0], value);
        decode_enumerated(&array[0], &decoded_value);
        ct_test(pTest, decoded_value == value);
        encode_tagged_enumerated(&encoded_array[0], decoded_value);
        ct_test(pTest, memcmp(&array[0], &encoded_array[0],
                sizeof(array)) == 0);
        // an enumerated will take up to 4 octects 
        // plus a one octet for the tag 
        apdu_len = encode_tagged_enumerated(&apdu[0], value);
        len = decode_tag_number_and_value(&apdu[0], &tag_number, NULL);
        ct_test(pTest, len == 1);
        ct_test(pTest, tag_number == BACNET_APPLICATION_TAG_ENUMERATED);
        ct_test(pTest, decode_is_context_specific(&apdu[0]) == false);
        // context specific encoding
        apdu_len = encode_context_enumerated(&apdu[0], 3, value);
        ct_test(pTest, decode_is_context_specific(&apdu[0]) == true);
        len = decode_tag_number_and_value(&apdu[0], &tag_number, NULL);
        ct_test(pTest, len == 1);
        ct_test(pTest, tag_number == 3);
        // test the interesting values
        value = value << 1;
    }

    return;
}

void testBACDCodeUnsigned(Test * pTest)
{
    uint8_t array[5] = { 0 };
    uint8_t encoded_array[5] = { 0 };
    unsigned int value = 1;
    unsigned int decoded_value = 0;
    int i, len, apdu_len;
    uint8_t apdu[MAX_APDU] = { 0 };
    uint8_t tag_number = 0;

    for (i = 0; i < 8; i++) {
        encode_tagged_unsigned(&array[0], value);
        decode_unsigned(&array[0], &decoded_value);
        ct_test(pTest, decoded_value == value);
        encode_tagged_unsigned(&encoded_array[0], decoded_value);
        ct_test(pTest, memcmp(&array[0], &encoded_array[0],
                sizeof(array)) == 0);
        // an unsigned will take up to 4 octects 
        // plus a one octet for the tag 
        apdu_len = encode_tagged_unsigned(&apdu[0], value);
        // apdu_len varies...
        //ct_test(pTest, apdu_len == 5);
        len = decode_tag_number_and_value(&apdu[0], &tag_number, NULL);
        ct_test(pTest, len == 1);
        ct_test(pTest, tag_number == BACNET_APPLICATION_TAG_UNSIGNED_INT);
        ct_test(pTest, decode_is_context_specific(&apdu[0]) == false);
        value = value << 1;
    }

    return;
}

void testBACDCodeSigned(Test * pTest)
{
    uint8_t array[5] = { 0 };
    uint8_t encoded_array[5] = { 0 };
    int value = 1;
    int decoded_value = 0;
    int i = 0, len = 0, apdu_len = 0;
    uint8_t apdu[MAX_APDU] = { 0 };
    uint8_t tag_number = 0;

    for (i = 0; i < 8; i++) {
        encode_tagged_signed(&array[0], value);
        decode_signed(&array[0], &decoded_value);
        ct_test(pTest, decoded_value == value);
        encode_tagged_signed(&encoded_array[0], decoded_value);
        ct_test(pTest, memcmp(&array[0], &encoded_array[0],
                sizeof(array)) == 0);
        // a signed int will take up to 4 octects 
        // plus a one octet for the tag 
        apdu_len = encode_tagged_signed(&apdu[0], value);
        len = decode_tag_number_and_value(&apdu[0], &tag_number, NULL);
        ct_test(pTest, tag_number == BACNET_APPLICATION_TAG_SIGNED_INT);
        ct_test(pTest, decode_is_context_specific(&apdu[0]) == false);
        value = value << 1;
    }

    value = -1;
    encode_tagged_signed(&array[0], value);
    decode_signed(&array[0], &decoded_value);
    ct_test(pTest, decoded_value == value);
    encode_tagged_signed(&encoded_array[0], decoded_value);
    ct_test(pTest, memcmp(&array[0], &encoded_array[0],
            sizeof(array)) == 0);
    // a signed int will take up to 4 octects 
    // plus a one octet for the tag 
    apdu_len = encode_tagged_signed(&apdu[0], value);
    len = decode_tag_number_and_value(&apdu[0], &tag_number, NULL);
    ct_test(pTest, len == 1);
    ct_test(pTest, tag_number == BACNET_APPLICATION_TAG_SIGNED_INT);
    ct_test(pTest, decode_is_context_specific(&apdu[0]) == false);

    return;
}

void testBACDCodeString(Test * pTest)
{
    uint8_t array[MAX_APDU] = { 0 };
    uint8_t encoded_array[MAX_APDU] = { 0 };
    char test_string[MAX_APDU] = { "" };
    char decoded_string[MAX_APDU] = { "" };
    int i;                      // for loop counter
    int apdu_len;
    int test_apdu_len;
    size_t len;
    char *test_string0 = "";

    apdu_len = encode_tagged_character_string(&array[0], &test_string0[0]);
    decode_character_string(&array[0], &decoded_string[0]);
    ct_test(pTest, apdu_len == 2);
    ct_test(pTest, strcmp(&test_string0[0], &decoded_string[0]) == 0);
    for (i = 0; i < (MAX_APDU - 6); i++) {
        test_string[i] = 'S';
        test_string[i + 1] = '\0';
        apdu_len =
            encode_tagged_character_string(&encoded_array[0], &test_string[0]);
        test_apdu_len =
            decode_character_string(&encoded_array[0], &decoded_string[0]);
        len = strlen(test_string);
        if (apdu_len != test_apdu_len) {
            printf("test string=#%d\n", i);
        }
        ct_test(pTest, apdu_len == test_apdu_len);
        if (strcmp(&test_string[0], &decoded_string[0]) != 0) {
            printf("test string=#%d\n", i);
        }
        ct_test(pTest, strcmp(&test_string[0], &decoded_string[0]) == 0);
    }

    return;
}

void testBACDCodeObject(Test * pTest)
{
    uint8_t object_array[4] = {
        0
    };
    uint8_t encoded_array[4] = {
        0
    };
    BACNET_OBJECT_TYPE type = OBJECT_BINARY_INPUT;
    BACNET_OBJECT_TYPE decoded_type = OBJECT_ANALOG_OUTPUT;
    int instance = 123;
    int decoded_instance = 0;

    encode_bacnet_object_id(&encoded_array[0], type, instance);
    decode_object_id(&encoded_array[0],
        (int *) &decoded_type, &decoded_instance);
    ct_test(pTest, decoded_type == type);
    ct_test(pTest, decoded_instance == instance);
    encode_bacnet_object_id(&object_array[0], type, instance);
    ct_test(pTest, memcmp(&object_array[0], &encoded_array[0],
            sizeof(object_array)) == 0);
    for (type = 0; type < 1024; type++) {
        for (instance = 0; instance <= 0x3FFFFF; instance += 1024) {
            encode_bacnet_object_id(&encoded_array[0], type, instance);
            decode_object_id(&encoded_array[0],
                (int *) &decoded_type, &decoded_instance);
            ct_test(pTest, decoded_type == type);
            ct_test(pTest, decoded_instance == instance);
            encode_bacnet_object_id(&object_array[0], type, instance);
            ct_test(pTest, memcmp(&object_array[0],
                    &encoded_array[0], sizeof(object_array)) == 0);
        }
    }

    return;
}

#ifdef TEST_DECODE
int main(void)
{
    Test *pTest;
    bool rc;

    pTest = ct_create("BACDCode", NULL);
    /* individual tests */
    rc = ct_addTestFunction(pTest, testBACDCodeTags);
    assert(rc);
    rc = ct_addTestFunction(pTest, testBACDCodeReal);
    assert(rc);
    rc = ct_addTestFunction(pTest, testBACDCodeUnsigned);
    assert(rc);
    rc = ct_addTestFunction(pTest, testBACDCodeSigned);
    assert(rc);
    rc = ct_addTestFunction(pTest, testBACDCodeEnumerated);
    assert(rc);
    rc = ct_addTestFunction(pTest, testBACDCodeString);
    assert(rc);
    rc = ct_addTestFunction(pTest, testBACDCodeObject);
    assert(rc);
    ct_setStream(pTest, stdout);
    ct_run(pTest);
    (void) ct_report(pTest);
    ct_destroy(pTest);

    return 0;
}
#endif                          /* TEST_DECODE */
#endif                          /* TEST */