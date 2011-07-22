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
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "bits.h"
#include "apdu.h"
#include "bacdef.h"
#include "bacdcode.h"
#include "bacenum.h"
#include "tsm.h"
#include "dcc.h"
#include "iam.h"
#include "session.h"
/** @file apdu.c  Handles APDU services */


#if 0
/* APDU Timeout in Milliseconds */
static uint16_t Timeout_Milliseconds = 3000;
/* Number of APDU Retries */
static uint8_t Number_Of_Retries = 3;
/* Confirmed Function Handlers */
/* If they are not set, they are handled by a reject message */
static confirmed_function Confirmed_Function[MAX_BACNET_CONFIRMED_SERVICE];
/* Allow the APDU handler to automatically reject */
static confirmed_function Unrecognized_Service_Handler;
/* Unconfirmed Function Handlers */
/* If they are not set, they are not handled */
static unconfirmed_function
    Unconfirmed_Function[MAX_BACNET_UNCONFIRMED_SERVICE];
/* Confirmed ACK Function Handlers */
static void *Confirmed_ACK_Function[MAX_BACNET_CONFIRMED_SERVICE];
/* Error Function Handler */
static error_function Error_Function[MAX_BACNET_CONFIRMED_SERVICE];
/* Abort Function Handler */
static abort_function Abort_Function;
/* Reject Function Handler */
static reject_function Reject_Function;
#endif

/* a simple table for crossing the services supported */
static BACNET_SERVICES_SUPPORTED
    confirmed_service_supported[MAX_BACNET_CONFIRMED_SERVICE] = {
    SERVICE_SUPPORTED_ACKNOWLEDGE_ALARM,
    SERVICE_SUPPORTED_CONFIRMED_COV_NOTIFICATION,
    SERVICE_SUPPORTED_CONFIRMED_EVENT_NOTIFICATION,
    SERVICE_SUPPORTED_GET_ALARM_SUMMARY,
    SERVICE_SUPPORTED_GET_ENROLLMENT_SUMMARY,
    SERVICE_SUPPORTED_SUBSCRIBE_COV,
    SERVICE_SUPPORTED_ATOMIC_READ_FILE,
    SERVICE_SUPPORTED_ATOMIC_WRITE_FILE,
    SERVICE_SUPPORTED_ADD_LIST_ELEMENT,
    SERVICE_SUPPORTED_REMOVE_LIST_ELEMENT,
    SERVICE_SUPPORTED_CREATE_OBJECT,
    SERVICE_SUPPORTED_DELETE_OBJECT,
    SERVICE_SUPPORTED_READ_PROPERTY,
    SERVICE_SUPPORTED_READ_PROP_CONDITIONAL,
    SERVICE_SUPPORTED_READ_PROP_MULTIPLE,
    SERVICE_SUPPORTED_WRITE_PROPERTY,
    SERVICE_SUPPORTED_WRITE_PROP_MULTIPLE,
    SERVICE_SUPPORTED_DEVICE_COMMUNICATION_CONTROL,
    SERVICE_SUPPORTED_PRIVATE_TRANSFER,
    SERVICE_SUPPORTED_TEXT_MESSAGE,
    SERVICE_SUPPORTED_REINITIALIZE_DEVICE,
    SERVICE_SUPPORTED_VT_OPEN,
    SERVICE_SUPPORTED_VT_CLOSE,
    SERVICE_SUPPORTED_VT_DATA,
    SERVICE_SUPPORTED_AUTHENTICATE,
    SERVICE_SUPPORTED_REQUEST_KEY,
    SERVICE_SUPPORTED_READ_RANGE,
    SERVICE_SUPPORTED_LIFE_SAFETY_OPERATION,
    SERVICE_SUPPORTED_SUBSCRIBE_COV_PROPERTY,
    SERVICE_SUPPORTED_GET_EVENT_INFORMATION
};

/* a simple table for crossing the services supported */
static BACNET_SERVICES_SUPPORTED
    unconfirmed_service_supported[MAX_BACNET_UNCONFIRMED_SERVICE] = {
    SERVICE_SUPPORTED_I_AM,
    SERVICE_SUPPORTED_I_HAVE,
    SERVICE_SUPPORTED_UNCONFIRMED_COV_NOTIFICATION,
    SERVICE_SUPPORTED_UNCONFIRMED_EVENT_NOTIFICATION,
    SERVICE_SUPPORTED_UNCONFIRMED_PRIVATE_TRANSFER,
    SERVICE_SUPPORTED_UNCONFIRMED_TEXT_MESSAGE,
    SERVICE_SUPPORTED_TIME_SYNCHRONIZATION,
    SERVICE_SUPPORTED_WHO_HAS,
    SERVICE_SUPPORTED_WHO_IS,
    SERVICE_SUPPORTED_UTC_TIME_SYNCHRONIZATION
};

void apdu_set_confirmed_handler(
    struct bacnet_session_object *session_object,
    BACNET_CONFIRMED_SERVICE service_choice,
    confirmed_function pFunction)
{
    if (service_choice < MAX_BACNET_CONFIRMED_SERVICE)
        session_object->APDU_Confirmed_Function[service_choice] = pFunction;
}

void apdu_set_unrecognized_service_handler_handler(
    struct bacnet_session_object *session_object,
    confirmed_function pFunction)
{
    session_object->APDU_Unrecognized_Service_Handler = pFunction;
}

void apdu_set_unconfirmed_handler(
    struct bacnet_session_object *session_object,
    BACNET_UNCONFIRMED_SERVICE service_choice,
    unconfirmed_function pFunction)
{
    if (service_choice < MAX_BACNET_UNCONFIRMED_SERVICE)
        session_object->APDU_Unconfirmed_Function[service_choice] = pFunction;
}

bool apdu_service_supported(
    struct bacnet_session_object *session_object,
    BACNET_SERVICES_SUPPORTED service_supported)
{
    int i = 0;
    bool status = false;
    bool found = false;

    if (service_supported < MAX_BACNET_SERVICES_SUPPORTED) {
        /* is it a confirmed service? */
        for (i = 0; i < MAX_BACNET_CONFIRMED_SERVICE; i++) {
            if (confirmed_service_supported[i] == service_supported) {
                if (session_object->APDU_Confirmed_Function[i] != NULL)
                    status = true;
                found = true;
                break;
            }
        }

        if (!found) {
            /* is it an unconfirmed service? */
            for (i = 0; i < MAX_BACNET_UNCONFIRMED_SERVICE; i++) {
                if (unconfirmed_service_supported[i] == service_supported) {
                    if (session_object->APDU_Unconfirmed_Function[i] != NULL)
                        status = true;
                    break;
                }
            }
        }
    }
    return status;
}

/** Function to translate a SERVICE_SUPPORTED_ enum to its SERVICE_CONFIRMED_
 *  or SERVICE_UNCONFIRMED_ index.
 *  Useful with the bactext_confirmed_service_name() functions.
 *
 * @param service_supported [in] The SERVICE_SUPPORTED_ enum value to convert.
 * @param index [out] The SERVICE_CONFIRMED_ or SERVICE_UNCONFIRMED_ index,
 *                    if found.
 * @param bIsConfirmed [out] True if index is a SERVICE_CONFIRMED_ type.
 * @return True if a match was found and index and bIsConfirmed are valid.
 */
bool apdu_service_supported_to_index(
    BACNET_SERVICES_SUPPORTED service_supported,
    size_t * index,
    bool * bIsConfirmed)
{
    int i = 0;
    bool found = false;

    *bIsConfirmed = false;
    if (service_supported < MAX_BACNET_SERVICES_SUPPORTED) {
        /* is it a confirmed service? */
        for (i = 0; i < MAX_BACNET_CONFIRMED_SERVICE; i++) {
            if (confirmed_service_supported[i] == service_supported) {
                found = true;
                *index = (size_t) i;
                *bIsConfirmed = true;
                break;
            }
        }

        if (!found) {
            /* is it an unconfirmed service? */
            for (i = 0; i < MAX_BACNET_UNCONFIRMED_SERVICE; i++) {
                if (unconfirmed_service_supported[i] == service_supported) {
                    found = true;
                    *index = (size_t) i;
                    break;
                }
            }
        }
    }
    return found;
}

void apdu_set_confirmed_simple_ack_handler(
    struct bacnet_session_object *session_object,
    BACNET_CONFIRMED_SERVICE service_choice,
    confirmed_simple_ack_function pFunction)
{
    switch (service_choice) {
        case SERVICE_CONFIRMED_ACKNOWLEDGE_ALARM:
        case SERVICE_CONFIRMED_COV_NOTIFICATION:
        case SERVICE_CONFIRMED_EVENT_NOTIFICATION:
        case SERVICE_CONFIRMED_SUBSCRIBE_COV:
        case SERVICE_CONFIRMED_SUBSCRIBE_COV_PROPERTY:
        case SERVICE_CONFIRMED_LIFE_SAFETY_OPERATION:
            /* Object Access Services */
        case SERVICE_CONFIRMED_ADD_LIST_ELEMENT:
        case SERVICE_CONFIRMED_REMOVE_LIST_ELEMENT:
        case SERVICE_CONFIRMED_DELETE_OBJECT:
        case SERVICE_CONFIRMED_WRITE_PROPERTY:
        case SERVICE_CONFIRMED_WRITE_PROP_MULTIPLE:
            /* Remote Device Management Services */
        case SERVICE_CONFIRMED_DEVICE_COMMUNICATION_CONTROL:
        case SERVICE_CONFIRMED_TEXT_MESSAGE:
        case SERVICE_CONFIRMED_REINITIALIZE_DEVICE:
            /* Virtual Terminal Services */
        case SERVICE_CONFIRMED_VT_CLOSE:
            /* Security Services */
        case SERVICE_CONFIRMED_REQUEST_KEY:
            session_object->APDU_Confirmed_ACK_Function[service_choice] =
                (void *) pFunction;
            break;
        default:
            break;
    }
}

void apdu_set_confirmed_ack_handler(
    struct bacnet_session_object *session_object,
    BACNET_CONFIRMED_SERVICE service_choice,
    confirmed_ack_function pFunction)
{
    switch (service_choice) {
        case SERVICE_CONFIRMED_GET_ALARM_SUMMARY:
        case SERVICE_CONFIRMED_GET_ENROLLMENT_SUMMARY:
        case SERVICE_CONFIRMED_GET_EVENT_INFORMATION:
            /* File Access Services */
        case SERVICE_CONFIRMED_ATOMIC_READ_FILE:
        case SERVICE_CONFIRMED_ATOMIC_WRITE_FILE:
            /* Object Access Services */
        case SERVICE_CONFIRMED_CREATE_OBJECT:
        case SERVICE_CONFIRMED_READ_PROPERTY:
        case SERVICE_CONFIRMED_READ_PROP_CONDITIONAL:
        case SERVICE_CONFIRMED_READ_PROP_MULTIPLE:
        case SERVICE_CONFIRMED_READ_RANGE:
            /* Remote Device Management Services */
        case SERVICE_CONFIRMED_PRIVATE_TRANSFER:
            /* Virtual Terminal Services */
        case SERVICE_CONFIRMED_VT_OPEN:
        case SERVICE_CONFIRMED_VT_DATA:
            /* Security Services */
        case SERVICE_CONFIRMED_AUTHENTICATE:
            session_object->APDU_Confirmed_ACK_Function[service_choice] =
                (void *) pFunction;
            break;
        default:
            break;
    }
}

void apdu_set_error_handler(
    struct bacnet_session_object *session_object,
    BACNET_CONFIRMED_SERVICE service_choice,
    error_function pFunction)
{
    if (service_choice < MAX_BACNET_CONFIRMED_SERVICE)
        session_object->APDU_Error_Function[service_choice] = pFunction;
}

void apdu_set_abort_handler(
    struct bacnet_session_object *session_object,
    abort_function pFunction)
{
    session_object->APDU_Abort_Function = pFunction;
}

void apdu_set_reject_handler(
    struct bacnet_session_object *session_object,
    reject_function pFunction)
{
    session_object->APDU_Reject_Function = pFunction;
}

uint16_t apdu_decode_confirmed_service_request(
    uint8_t * apdu,     /* APDU data */
    uint16_t apdu_len,
    BACNET_CONFIRMED_SERVICE_DATA * service_data,
    uint8_t * service_choice,
    uint8_t ** service_request,
    uint32_t * service_request_len)
{
    uint16_t len = 0;   /* counts where we are in PDU */

    service_data->segmented_message = (apdu[0] & BIT3) ? true : false;
    service_data->more_follows = (apdu[0] & BIT2) ? true : false;
    service_data->segmented_response_accepted =
        (apdu[0] & BIT1) ? true : false;
    service_data->max_segs = decode_max_segs(apdu[1]);
    service_data->max_resp = decode_max_apdu(apdu[1]);
    service_data->invoke_id = apdu[2];
    len = 3;
    if (service_data->segmented_message) {
        service_data->sequence_number = apdu[len++];
        service_data->proposed_window_number = apdu[len++];
    }
    *service_choice = apdu[len++];
    *service_request = &apdu[len];
    *service_request_len = apdu_len - len;

    return len;
}

void apdu_init_fixed_header(
    BACNET_APDU_FIXED_HEADER * fixed_pdu_header,
    uint8_t pdu_type,
    uint8_t invoke_id,
    uint8_t service,
    int max_apdu)
{
    fixed_pdu_header->pdu_type = pdu_type;

    fixed_pdu_header->service_data.common_data.invoke_id = invoke_id;
    fixed_pdu_header->service_data.common_data.more_follows = false;
    fixed_pdu_header->service_data.common_data.proposed_window_number = 0;
    fixed_pdu_header->service_data.common_data.sequence_number = 0;
    fixed_pdu_header->service_data.common_data.segmented_message = false;
    switch (pdu_type) {
        case PDU_TYPE_CONFIRMED_SERVICE_REQUEST:
            fixed_pdu_header->service_data.request_data.max_segs =
                MAX_SEGMENTS_ACCEPTED;
            /* allow to specify a lower APDU size : support arbitrary reduction of APDU packets between peers */
            fixed_pdu_header->service_data.request_data.max_resp =
                max_apdu < MAX_APDU ? max_apdu : MAX_APDU;
            fixed_pdu_header->service_data.request_data.
                segmented_response_accepted = MAX_SEGMENTS_ACCEPTED > 1;
            break;
        case PDU_TYPE_COMPLEX_ACK:
            break;
    }
    fixed_pdu_header->service_choice = service;
    /* structure order checks */
    assert(&fixed_pdu_header->service_data.ack_data.invoke_id ==
        &fixed_pdu_header->service_data.request_data.invoke_id);
    assert(&fixed_pdu_header->service_data.ack_data.more_follows ==
        &fixed_pdu_header->service_data.request_data.more_follows);
    assert(&fixed_pdu_header->service_data.ack_data.proposed_window_number ==
        &fixed_pdu_header->service_data.request_data.proposed_window_number);
    assert(&fixed_pdu_header->service_data.ack_data.sequence_number ==
        &fixed_pdu_header->service_data.request_data.sequence_number);
    assert(&fixed_pdu_header->service_data.ack_data.segmented_message ==
        &fixed_pdu_header->service_data.request_data.segmented_message);
}

int apdu_encode_fixed_header(
    uint8_t * apdu,
    int max_apdu_length,
    BACNET_APDU_FIXED_HEADER * fixed_pdu_header)
{
    int apdu_len = 0;
    switch (fixed_pdu_header->pdu_type) {
        case PDU_TYPE_CONFIRMED_SERVICE_REQUEST:
            apdu[apdu_len++] = fixed_pdu_header->pdu_type
                /* flag 'SA' if we accept many segments */
                | (fixed_pdu_header->service_data.request_data.
                segmented_response_accepted ? 0x02 : 0x00)
                /* flag 'MOR' if we more segments are coming */
                | (fixed_pdu_header->service_data.request_data.
                more_follows ? 0x04 : 0x00)
                /* flag 'SEG' if we more segments are coming */
                | (fixed_pdu_header->service_data.request_data.
                segmented_message ? 0x08 : 0x00);
            apdu[apdu_len++] =
                encode_max_segs_max_apdu(fixed_pdu_header->service_data.
                request_data.max_segs,
                fixed_pdu_header->service_data.request_data.max_resp);
            apdu[apdu_len++] =
                fixed_pdu_header->service_data.request_data.invoke_id;
            /* extra data for segmented messages sending */
            if (fixed_pdu_header->service_data.request_data.segmented_message) {
                /* packet sequence number */
                apdu[apdu_len++] =
                    fixed_pdu_header->service_data.request_data.
                    sequence_number;
                /* window size proposal */
                apdu[apdu_len++] =
                    fixed_pdu_header->service_data.request_data.
                    proposed_window_number;
            }
            /* service choice */
            apdu[apdu_len++] = fixed_pdu_header->service_choice;
            break;
        case PDU_TYPE_COMPLEX_ACK:
            apdu[apdu_len++] = fixed_pdu_header->pdu_type
                /* flag 'MOR' if we more segments are coming */
                | (fixed_pdu_header->service_data.ack_data.
                more_follows ? 0x04 : 0x00)
                /* flag 'SEG' if we more segments are coming */
                | (fixed_pdu_header->service_data.ack_data.
                segmented_message ? 0x08 : 0x00);
            apdu[apdu_len++] =
                fixed_pdu_header->service_data.ack_data.invoke_id;
            /* extra data for segmented messages sending */
            if (fixed_pdu_header->service_data.ack_data.segmented_message) {
                /* packet sequence number */
                apdu[apdu_len++] =
                    fixed_pdu_header->service_data.ack_data.sequence_number;
                /* window size proposal */
                apdu[apdu_len++] =
                    fixed_pdu_header->service_data.ack_data.
                    proposed_window_number;
            }
            /* service choice */
            apdu[apdu_len++] = fixed_pdu_header->service_choice;
            break;
    }
    return apdu_len;
}


uint16_t apdu_timeout(
    struct bacnet_session_object * session_object)
{
    return session_object->APDU_Timeout_Milliseconds;
}

void apdu_timeout_set(
    struct bacnet_session_object *session_object,
    uint16_t milliseconds)
{
    session_object->APDU_Timeout_Milliseconds = milliseconds;
}

uint16_t apdu_segment_timeout(
    struct bacnet_session_object *session_object)
{
    return session_object->APDU_Segment_Timeout_Milliseconds;
}

void apdu_segment_timeout_set(
    struct bacnet_session_object *session_object,
    uint16_t milliseconds)
{
    session_object->APDU_Segment_Timeout_Milliseconds = milliseconds;
}

uint8_t apdu_retries(
    struct bacnet_session_object *session_object)
{
    return session_object->APDU_Number_Of_Retries;
}

void apdu_retries_set(
    struct bacnet_session_object *session_object,
    uint8_t value)
{
    session_object->APDU_Number_Of_Retries = value;
}

/* Invoke special handler for confirmed service */
void invoke_confirmed_service_service_request(
    struct bacnet_session_object *session_object,
    BACNET_ADDRESS * src,
    BACNET_CONFIRMED_SERVICE_DATA * service_data,
    uint8_t service_choice,
    uint8_t * service_request,
    uint32_t service_request_len)
{
    /* When network communications are completely disabled,
       only DeviceCommunicationControl and ReinitializeDevice APDUs
       shall be processed and no messages shall be initiated. */
    if (dcc_communication_disabled(session_object) &&
        ((service_choice != SERVICE_CONFIRMED_DEVICE_COMMUNICATION_CONTROL)
            && (service_choice != SERVICE_CONFIRMED_REINITIALIZE_DEVICE)))
        return;
    if ((service_choice < MAX_BACNET_CONFIRMED_SERVICE) &&
        (session_object->APDU_Confirmed_Function[service_choice]))
        session_object->APDU_Confirmed_Function[service_choice]
            (session_object, service_request, service_request_len, src,
            service_data);
    else if (session_object->APDU_Unrecognized_Service_Handler)
        session_object->APDU_Unrecognized_Service_Handler(session_object,
            service_request, service_request_len, src, service_data);
}

/* Handler for normal message without segmentation, or segmented complete message reassembled all-in-one */
void apdu_handler_confirmed_service(
    struct bacnet_session_object *session_object,
    BACNET_ADDRESS * src,
    uint8_t * apdu,     /* APDU data */
    uint32_t apdu_len)
{
    BACNET_CONFIRMED_SERVICE_DATA service_data = { 0 };
    uint8_t service_choice = 0;
    uint8_t *service_request = NULL;
    uint32_t service_request_len = 0;
    uint32_t len = 0;   /* counts where we are in PDU */

    len = apdu_decode_confirmed_service_request(&apdu[0],       /* APDU data */
        apdu_len, &service_data, &service_choice, &service_request,
        &service_request_len);

    /* we could check if TSM state is Ok, and send Abort otherwise */
    /* if ( tsm_set_confirmed_service_received(session_object, src, &service_data) ) */
    invoke_confirmed_service_service_request(session_object, src,
        &service_data, service_choice, service_request, service_request_len);
}

/** Handler for messages with segmentation :
   - store new packet if sequence number is ok
   - NACK packet if sequence number is not ok
   - call the final functions with reassembled data when last packet ok is received
*/
void apdu_handler_confirmed_service_segment(
    struct bacnet_session_object *session_object,
    BACNET_ADDRESS * src,
    uint8_t * apdu,     /* APDU data */
    uint32_t apdu_len)
{
    BACNET_CONFIRMED_SERVICE_DATA service_data = { 0 };
    uint8_t internal_service_id = 0;
    uint8_t service_choice = 0;
    uint8_t *service_request = NULL;
    uint32_t service_request_len = 0;
    uint32_t len = 0;   /* counts where we are in PDU */
    bool segment_ok;

    len = apdu_decode_confirmed_service_request(&apdu[0],       /* APDU data */
        apdu_len, &service_data, &service_choice, &service_request,
        &service_request_len);

    /* new segment : memorize it */
    segment_ok =
        tsm_set_segmented_confirmed_service_received(session_object, src,
        &service_data, &internal_service_id, &service_request,
        &service_request_len);
    /* last segment  */
    if (segment_ok && !service_data.more_follows) {
        /* Clear peer informations */
        tsm_clear_peer_id(session_object, internal_service_id);
        /* Invoke service handler */
        invoke_confirmed_service_service_request(session_object, src,
            &service_data, service_choice, service_request,
            service_request_len);
        /* We must free invoke_id, and associated data */
        tsm_free_invoke_id_check(session_object, internal_service_id, NULL,
            true);
    }
}


void invoke_complex_ack_service_request(
    struct bacnet_session_object *session_object,
    BACNET_ADDRESS * src,
    uint8_t invoke_id,
    BACNET_CONFIRMED_SERVICE_ACK_DATA * service_ack_data,
    uint8_t service_choice,
    uint8_t * service_request,
    uint32_t service_request_len)
{
    switch (service_choice) {
        case SERVICE_CONFIRMED_GET_ALARM_SUMMARY:
        case SERVICE_CONFIRMED_GET_ENROLLMENT_SUMMARY:
        case SERVICE_CONFIRMED_GET_EVENT_INFORMATION:
            /* File Access Services */
        case SERVICE_CONFIRMED_ATOMIC_READ_FILE:
        case SERVICE_CONFIRMED_ATOMIC_WRITE_FILE:
            /* Object Access Services */
        case SERVICE_CONFIRMED_CREATE_OBJECT:
        case SERVICE_CONFIRMED_READ_PROPERTY:
        case SERVICE_CONFIRMED_READ_PROP_CONDITIONAL:
        case SERVICE_CONFIRMED_READ_PROP_MULTIPLE:
        case SERVICE_CONFIRMED_READ_RANGE:
        case SERVICE_CONFIRMED_PRIVATE_TRANSFER:
            /* Virtual Terminal Services */
        case SERVICE_CONFIRMED_VT_OPEN:
        case SERVICE_CONFIRMED_VT_DATA:
            /* Security Services */
        case SERVICE_CONFIRMED_AUTHENTICATE:
            if (session_object->APDU_Confirmed_ACK_Function[service_choice]) {
                ((confirmed_ack_function)
                    session_object->APDU_Confirmed_ACK_Function
                    [service_choice])
                    (session_object, service_request, service_request_len, src,
                    service_ack_data);
            }
            tsm_free_invoke_id_check(session_object, invoke_id, src, true);
            break;
        default:
            break;
    }
}

/* Handler for normal message without segmentation, or segmented complete message reassembled all-in-one */
void apdu_handler_complex_ack(
    struct bacnet_session_object *session_object,
    BACNET_ADDRESS * src,
    uint8_t * apdu,     /* APDU data */
    uint32_t apdu_len)
{
    BACNET_CONFIRMED_SERVICE_ACK_DATA service_ack_data = { 0 };
    uint8_t invoke_id = 0;
    uint8_t service_choice = 0;
    uint8_t *service_request = NULL;
    uint32_t service_request_len = 0;
    uint32_t len = 0;   /* counts where we are in PDU */

    service_ack_data.segmented_message = (apdu[0] & BIT3) ? true : false;
    service_ack_data.more_follows = (apdu[0] & BIT2) ? true : false;
    invoke_id = service_ack_data.invoke_id = apdu[1];
    len = 2;
    if (service_ack_data.segmented_message) {
        service_ack_data.sequence_number = apdu[len++];
        service_ack_data.proposed_window_number = apdu[len++];
    }

    service_choice = apdu[len++];
    service_request = &apdu[len];
    service_request_len = apdu_len - len;

    /* invoke callback if TSM state is Ok, send Abort otherwise */
    if (tsm_set_complexack_received(session_object, src, &service_ack_data))
        invoke_complex_ack_service_request(session_object, src, invoke_id,
            &service_ack_data, service_choice, service_request,
            service_request_len);
}

/** Handler for messages with segmentation :
   - store new packet if sequence number is ok
   - NACK packet if sequence number is not ok
   - call the final functions with reassembled data when last packet ok is received
*/
void apdu_handler_complex_ack_segment(
    struct bacnet_session_object *session_object,
    BACNET_ADDRESS * src,
    uint8_t * apdu,     /* APDU data */
    uint32_t apdu_len)
{
    BACNET_CONFIRMED_SERVICE_ACK_DATA service_ack_data = { 0 };
    uint8_t invoke_id = 0;
    uint8_t service_choice = 0;
    uint8_t *service_request = NULL;
    uint32_t service_request_len = 0;
    uint32_t len = 0;   /* counts where we are in PDU */
    bool segment_ok;

    service_ack_data.segmented_message = (apdu[0] & BIT3) ? true : false;
    service_ack_data.more_follows = (apdu[0] & BIT2) ? true : false;
    invoke_id = service_ack_data.invoke_id = apdu[1];
    len = 2;
    if (service_ack_data.segmented_message) {
        service_ack_data.sequence_number = apdu[len++];
        service_ack_data.proposed_window_number = apdu[len++];
    }

    service_choice = apdu[len++];
    service_request = &apdu[len];
    service_request_len = apdu_len - len;

    /* new segment : memorize it */
    segment_ok =
        tsm_set_segmentedcomplexack_received(session_object, src,
        &service_ack_data, &service_request, &service_request_len);
    /* last segment  */
    if (segment_ok && !service_ack_data.more_follows) {
        invoke_complex_ack_service_request(session_object, src, invoke_id,
            &service_ack_data, service_choice, service_request,
            service_request_len);
    }
}

void apdu_handler(
    struct bacnet_session_object *session_object,
    BACNET_ADDRESS * src,
    uint8_t * apdu,     /* APDU data */
    uint16_t apdu_len)
{
    uint8_t invoke_id = 0;
    uint8_t service_choice = 0;
    uint8_t *service_request = NULL;
    uint16_t service_request_len = 0;
    uint16_t len = 0;   /* counts where we are in PDU */
    uint8_t tag_number = 0;
    uint32_t len_value = 0;
    uint32_t error_code = 0;
    uint32_t error_class = 0;
    uint8_t reason = 0;
    uint8_t sequence_number = 0;
    uint8_t actual_window_size = 0;
    bool server = false;
    bool nak = false;

    if (apdu) {
        /* PDU Type */
        switch (apdu[0] & 0xF0) {
            case PDU_TYPE_CONFIRMED_SERVICE_REQUEST:
                /* segmented_message_reception ? */
                if (apdu[0] & BIT3)
                    apdu_handler_confirmed_service_segment(session_object, src,
                        apdu, apdu_len);
                else
                    apdu_handler_confirmed_service(session_object, src, apdu,
                        apdu_len);
                break;
            case PDU_TYPE_UNCONFIRMED_SERVICE_REQUEST:
                if (dcc_communication_disabled(session_object))
                    break;
                service_choice = apdu[1];
                service_request = &apdu[2];
                service_request_len = apdu_len - 2;
                if (service_choice < MAX_BACNET_UNCONFIRMED_SERVICE) {
                    if (session_object->APDU_Unconfirmed_Function
                        [service_choice])
                        session_object->APDU_Unconfirmed_Function
                            [service_choice]
                            (session_object, service_request,
                            service_request_len, src);
                }
                break;
            case PDU_TYPE_SIMPLE_ACK:
                invoke_id = apdu[1];
                service_choice = apdu[2];
                /* check if current TSM state is correct, ABORT otherwise */
                if (tsm_set_simpleack_received(session_object, invoke_id, src))
                    switch (service_choice) {
                        case SERVICE_CONFIRMED_ACKNOWLEDGE_ALARM:
                        case SERVICE_CONFIRMED_COV_NOTIFICATION:
                        case SERVICE_CONFIRMED_EVENT_NOTIFICATION:
                        case SERVICE_CONFIRMED_SUBSCRIBE_COV:
                        case SERVICE_CONFIRMED_SUBSCRIBE_COV_PROPERTY:
                        case SERVICE_CONFIRMED_LIFE_SAFETY_OPERATION:
                            /* Object Access Services */
                        case SERVICE_CONFIRMED_ADD_LIST_ELEMENT:
                        case SERVICE_CONFIRMED_REMOVE_LIST_ELEMENT:
                        case SERVICE_CONFIRMED_DELETE_OBJECT:
                        case SERVICE_CONFIRMED_WRITE_PROPERTY:
                        case SERVICE_CONFIRMED_WRITE_PROP_MULTIPLE:
                            /* Remote Device Management Services */
                        case SERVICE_CONFIRMED_DEVICE_COMMUNICATION_CONTROL:
                        case SERVICE_CONFIRMED_REINITIALIZE_DEVICE:
                        case SERVICE_CONFIRMED_TEXT_MESSAGE:
                            /* Virtual Terminal Services */
                        case SERVICE_CONFIRMED_VT_CLOSE:
                            /* Security Services */
                        case SERVICE_CONFIRMED_REQUEST_KEY:
                            if (session_object->APDU_Confirmed_ACK_Function
                                [service_choice]) {
                                ((confirmed_simple_ack_function)
                                    session_object->APDU_Confirmed_ACK_Function
                                    [service_choice])
                                    (session_object, src, invoke_id);
                            }
                            break;
                        default:
                            break;
                    }
                tsm_free_invoke_id_check(session_object, invoke_id, src, true);
                break;
            case PDU_TYPE_COMPLEX_ACK:
                {
                    /* segmented_message_reception ? */
                    if (apdu[0] & BIT3)
                        apdu_handler_complex_ack_segment(session_object, src,
                            apdu, apdu_len);
                    else
                        apdu_handler_complex_ack(session_object, src, apdu,
                            apdu_len);
                    break;
                }
                break;
            case PDU_TYPE_SEGMENT_ACK:
                if (apdu_len < 4)
                    break;
                server = apdu[0] & 0x01;
                nak = apdu[0] & 0x02;
                invoke_id = apdu[1];
                sequence_number = apdu[2];
                actual_window_size = apdu[3];
                /* we care because we support segmented message sending */
                tsm_segmentack_received(session_object, invoke_id,
                    sequence_number, actual_window_size, nak, server, src);
                break;
            case PDU_TYPE_ERROR:
                invoke_id = apdu[1];
                service_choice = apdu[2];
                len = 3;

                /* FIXME: Currently special case for C_P_T but there are others which may
                   need consideration such as ChangeList-Error, CreateObject-Error,
                   WritePropertyMultiple-Error and VTClose_Error but they may be left as
                   is for now until support for these services is added */

                if (service_choice == SERVICE_CONFIRMED_PRIVATE_TRANSFER) {     /* skip over opening tag 0 */
                    if (decode_is_opening_tag_number(&apdu[len], 0)) {
                        len++;  /* a tag number of 0 is not extended so only one octet */
                    }
                }
                len +=
                    decode_tag_number_and_value(&apdu[len], &tag_number,
                    &len_value);
                /* FIXME: we could validate that the tag is enumerated... */
                len += decode_enumerated(&apdu[len], len_value, &error_class);
                len +=
                    decode_tag_number_and_value(&apdu[len], &tag_number,
                    &len_value);
                /* FIXME: we could validate that the tag is enumerated... */
                len += decode_enumerated(&apdu[len], len_value, &error_code);

                if (service_choice == SERVICE_CONFIRMED_PRIVATE_TRANSFER) {     /* skip over closing tag 0 */
                    if (decode_is_closing_tag_number(&apdu[len], 0)) {
                        len++;  /* a tag number of 0 is not extended so only one octet */
                    }
                }
                if (service_choice < MAX_BACNET_CONFIRMED_SERVICE) {
                    if (session_object->APDU_Error_Function[service_choice])
                        session_object->APDU_Error_Function[service_choice]
                            (session_object, src, invoke_id,
                            (BACNET_ERROR_CLASS) error_class,
                            (BACNET_ERROR_CODE) error_code);
                }
                tsm_error_received(session_object, invoke_id, src);
                tsm_free_invoke_id_check(session_object, invoke_id, src, true);
                break;
            case PDU_TYPE_REJECT:
                invoke_id = apdu[1];
                reason = apdu[2];
                if (session_object->APDU_Reject_Function)
                    session_object->APDU_Reject_Function(session_object, src,
                        invoke_id, reason);
                tsm_reject_received(session_object, invoke_id, src);
                tsm_free_invoke_id_check(session_object, invoke_id, src, true);
                break;
            case PDU_TYPE_ABORT:
                server = apdu[0] & 0x01;
                invoke_id = apdu[1];
                reason = apdu[2];
                if (session_object->APDU_Abort_Function)
                    session_object->APDU_Abort_Function(session_object, src,
                        invoke_id, reason, server);
                tsm_free_invoke_id_check(session_object, invoke_id, src, true);
                break;
            default:
                break;
        }
    }
    return;
}