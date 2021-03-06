/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/**
 * \file operating.c
 * \brief Operating module source.
 * Copyright (C) 2010 Signove Tecnologia Corporation.
 * All rights reserved.
 * Contact: Signove Tecnologia Corporation (contact@signove.com)
 *
 * $LICENSE_TEXT:BEGIN$
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation and appearing
 * in the file LICENSE included in the packaging of this file; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
 * $LICENSE_TEXT:END$
 *
 * IEEE 11073 Communication Model - Finite State Machine implementation
 *
 * \author Diego Bezerra, Mateus Lima
 * \date Jun 23, 2010
 */

/**
 * \defgroup Operating Operating
 * \brief Operating module.
 * \ingroup FSM
 * @{
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/manager_p.h"
#include "src/communication/common/service.h"
#include "src/communication/common/communication.h"
#include "src/communication/manager/manager_operating.h"
#include "src/communication/manager/manager_disassociating.h"
#include "src/communication/parser/struct_cleaner.h"
#include "src/util/bytelib.h"
#include "src/communication/parser/decoder_ASN1.h"
#include "src/communication/parser/encoder_ASN1.h"
#include "src/dim/mds.h"
#include "src/dim/epi_cfg_scanner.h"
#include "src/dim/peri_cfg_scanner.h"
#include "src/dim/pmstore.h"
#include "src/dim/nomenclature.h"
#include "src/util/ioutil.h"
#include "src/util/log.h"
#include "src/communication/common/communication.h"

static void communication_process_roiv(Context *ctx, APDU *apdu);

static void communication_process_roer(Context *ctx, APDU *apdu);
static void communication_process_rors(Context *ctx, APDU *apdu);
static void communication_process_rorj(Context *ctx, APDU *apdu);

void operating_decode_epi_scan_event(Context *ctx, struct EpiCfgScanner *scanner, OID_Type event_type, Any *event);

void operating_decode_peri_scan_event(Context *ctx, struct PeriCfgScanner *scanner, OID_Type event_type, Any *event);

void operating_decode_trig_segment_data_xfer_response(struct MDS *mds, Any *event, ASN1_HANDLE obj_handle,
							Request *r, int err, int errcode);

void operating_decode_clear_segment(struct MDS *mds, ASN1_HANDLE obj_handle, Request *r,
					int err, int errcode);

/**
 * Get original ROIV request type, if Request was a ROIV
 * @param r Request
 * @return request code or -1 if not found
 */
static int get_roiv_choice(Request* r)
{
	int choice = -1;

	if (! r->apdu) {
		DEBUG("Request has no APDU");
		goto epilogue;
	}

	if (r->apdu->choice != PRST_CHOSEN) {
		DEBUG("Request is not PRST");
		goto epilogue;
	}

	DATA_apdu *data_apdu = encode_get_data_apdu(&r->apdu->u.prst);
	Data_apdu_message *msg = &data_apdu->message;
	choice = msg->choice;

epilogue:
	return choice;
}

/**
 * Get original ROIV request code, if Request was a ROIV
 * @param r Request
 * @return request code or -1 if not found
 */
static int get_roiv_action_type(Request* r)
{
	int action = -1;

	int c = get_roiv_choice(r);
	if (c < 0) {
		goto epilogue;
	}

	DATA_apdu *data_apdu = encode_get_data_apdu(&r->apdu->u.prst);
	Data_apdu_message *msg = &data_apdu->message;

	if (c == ROIV_CMIP_EVENT_REPORT_CHOSEN) {
		// action = msg->u.roiv_cmipEventReport.event_type;
	} else if (c == ROIV_CMIP_CONFIRMED_EVENT_REPORT_CHOSEN) {
		// action = msg->u.roiv_cmipConfirmedEventReport.event_type;
	} else if (c == ROIV_CMIP_GET_CHOSEN) {
		// no type
	} else if (c == ROIV_CMIP_SET_CHOSEN) {
		// no type
	} else if (c == ROIV_CMIP_CONFIRMED_SET_CHOSEN) {
		// no type
	} else if (c == ROIV_CMIP_ACTION_CHOSEN) {
		action = msg->u.roiv_cmipAction.action_type;
	} else if (c == ROIV_CMIP_CONFIRMED_ACTION_CHOSEN) {
		action = msg->u.roiv_cmipConfirmedAction.action_type;
	}

epilogue:
	return action;
}

/**
 * Get original ROIV object handle, if Request was a ROIV
 * @param r Request
 * @return request code or -1 if not found
 */
static int get_roiv_object_handle(Request* r)
{
	int handle = -1;

	int c = get_roiv_choice(r);
	if (c < 0) {
		goto epilogue;
	}

	DATA_apdu *data_apdu = encode_get_data_apdu(&r->apdu->u.prst);
	Data_apdu_message *msg = &data_apdu->message;

	if (c == ROIV_CMIP_EVENT_REPORT_CHOSEN) {
		handle = msg->u.roiv_cmipEventReport.obj_handle;
	} else if (c == ROIV_CMIP_CONFIRMED_EVENT_REPORT_CHOSEN) {
		handle = msg->u.roiv_cmipConfirmedEventReport.obj_handle;
	} else if (c == ROIV_CMIP_GET_CHOSEN) {
		handle = msg->u.roiv_cmipGet.obj_handle;
	} else if (c == ROIV_CMIP_SET_CHOSEN) {
		handle = msg->u.roiv_cmipSet.obj_handle;
	} else if (c == ROIV_CMIP_CONFIRMED_SET_CHOSEN) {
		handle = msg->u.roiv_cmipConfirmedSet.obj_handle;
	} else if (c == ROIV_CMIP_ACTION_CHOSEN) {
		handle = msg->u.roiv_cmipAction.obj_handle;
	} else if (c == ROIV_CMIP_CONFIRMED_ACTION_CHOSEN) {
		handle = msg->u.roiv_cmipConfirmedAction.obj_handle;
	}

epilogue:
	return handle;
}

/**
 * Process incoming APDU
 *
 * @param *apdu
 */
void operating_process_apdu(Context *ctx, APDU *apdu)
{
	//RejectResult reject_result;

	switch (apdu->choice) {
	case PRST_CHOSEN: {
		DATA_apdu *data_apdu = encode_get_data_apdu(&apdu->u.prst);

		if (communication_is_roiv_type(data_apdu)) {
			DEBUG("*** Received ROIV apdu ***");
			communication_process_roiv(ctx, apdu);
		} else if (communication_is_rors_type(data_apdu)) {
			DEBUG("*** Received RORS apdu ***");
			communication_process_rors(ctx, apdu);
		} else if (communication_is_roer_type(data_apdu)) {
			DEBUG("*** Received ROER apdu ***");
			communication_process_roer(ctx, apdu);
		} else if (communication_is_rorj_type(data_apdu)) {
			DEBUG("*** Received RORJ apdu ***");
			communication_process_rorj(ctx, apdu);
		}

		break;
	}
	case ABRT_CHOSEN:
		communication_fire_evt(ctx, fsm_evt_rx_abrt, NULL);
		break;
	case RLRQ_CHOSEN: {
		FSMEventData evt;
		evt.choice = FSM_EVT_DATA_RELEASE_RESPONSE_REASON;
		evt.u.release_response_reason = RELEASE_RESPONSE_REASON_NORMAL;
		communication_fire_evt(ctx, fsm_evt_rx_rlrq, &evt);
		break;
	}
	case RLRE_CHOSEN: {
		communication_fire_evt(ctx, fsm_evt_rx_rlre, NULL);
		break;
	}
	case AARQ_CHOSEN: {
		communication_fire_evt(ctx, fsm_evt_rx_aarq, NULL);
		break;
	}
	case AARE_CHOSEN: {
		communication_fire_evt(ctx, fsm_evt_rx_aare, NULL);
		break;
	}
	default:
		DEBUG("Unknown APDU type %d, aborting", apdu->choice);
		communication_fire_evt(ctx, fsm_evt_req_assoc_abort, NULL);
		break;
	}
}

/**
 * Process roiv in APDU
 *
 * @param ctx the current context.
 * @param *apdu
 */
void communication_process_roiv(Context *ctx, APDU *apdu)

{
	DATA_apdu *data_apdu = encode_get_data_apdu(&apdu->u.prst);
	FSMEventData data;
	//RejectResult reject_result;

	switch (data_apdu->message.choice) {
	case ROIV_CMIP_EVENT_REPORT_CHOSEN:
		DEBUG("received ROIV_CMIP_EVENT_REPORT_CHOSEN ");
		data.received_apdu = apdu;
		communication_fire_evt(ctx, fsm_evt_rx_roiv_event_report, &data);
		break;
	case ROIV_CMIP_CONFIRMED_EVENT_REPORT_CHOSEN:
		data.received_apdu = apdu;
		communication_fire_evt(ctx, fsm_evt_rx_roiv_confirmed_event_report,
				       &data);
		break;
	default:
		data.received_apdu = apdu;
		communication_fire_evt(ctx, fsm_evt_rx_roiv_all_except_confirmed_event_report,
				       &data);
		break;
	}
}

/**
 * Process rorj in APDU
 *
 * @param ctx
 * @param *apdu
 */
static void communication_process_rorj(Context *ctx, APDU *apdu)
{
	DATA_apdu *data_apdu = encode_get_data_apdu(&apdu->u.prst);
	FSMEventData data;

	if (service_check_known_invoke_id(ctx, data_apdu)) {
		data.received_apdu = apdu;
		communication_fire_evt(ctx, fsm_evt_rx_rorj, &data);
		service_request_retired(ctx, data_apdu);
	}
}


/**
 * Process roer in APDU
 *
 * @param ctx
 * @param *apdu
 */
static void communication_process_roer(Context *ctx, APDU *apdu)
{
	DATA_apdu *data_apdu = encode_get_data_apdu(&apdu->u.prst);
	FSMEventData data;

	if (service_check_known_invoke_id(ctx, data_apdu)) {
		DEBUG("Known Invoke ID");
		data.received_apdu = apdu;
		communication_fire_evt(ctx, fsm_evt_rx_roer, &data);
		service_request_retired(ctx, data_apdu);
	}
}

/**
 * Process rors in APDU
 *
 * @param ctx
 * @param *apdu
 */
static void communication_process_rors(Context *ctx, APDU *apdu)
{
	DATA_apdu *data_apdu = encode_get_data_apdu(&apdu->u.prst);
	FSMEventData data;
	//RejectResult reject_result;

	if (service_check_known_invoke_id(ctx, data_apdu)) {
		switch (data_apdu->message.choice) {
		case RORS_CMIP_GET_CHOSEN: // 8.26
			data.received_apdu = apdu;
			communication_fire_evt(ctx, fsm_evt_rx_rors_get, &data);
			break;
		case RORS_CMIP_CONFIRMED_ACTION_CHOSEN: // 8.26
			data.received_apdu = apdu;
			communication_fire_evt(ctx,
					       fsm_evt_rx_rors_confirmed_action, &data);
			break;
		case RORS_CMIP_CONFIRMED_SET_CHOSEN: // 8.26
			data.received_apdu = apdu;
			communication_fire_evt(ctx, fsm_evt_rx_rors_confirmed_set,
					       &data);
			break;
		default:
			DEBUG("Unknown received RORS choice: %d",
				data_apdu->message.choice);
			break;
		}

		service_request_retired(ctx, data_apdu);
	}
}

/**
 * Send reject to ROIV that is not an event report
 *
 * @param ctx context
 * @param evt
 * @param data
 */
void operating_roiv_non_event_report(Context *ctx, fsm_events evt, FSMEventData *data)
{
	data->choice = FSM_EVT_DATA_REJECT_RESULT;
	data->u.reject_result.problem = UNRECOGNIZED_OPERATION;
	communication_rorj_tx(ctx, evt, data);
}

/**
 * Process event reports
 *
 * @param ctx context
 * @param evt
 * @param data
 */
void operating_event_report(Context *ctx, fsm_events evt, FSMEventData *data)
{
	DATA_apdu *data_apdu = encode_get_data_apdu(&data->received_apdu->u.prst);

	Any event;
	OID_Type type;
	ASN1_HANDLE handle;
	RelativeTime time;

	if (data_apdu->message.choice == ROIV_CMIP_EVENT_REPORT_CHOSEN) {
		DEBUG(" operating_event_report ");
		event = data_apdu->message.u.roiv_cmipEventReport.event_info;
		type = data_apdu->message.u.roiv_cmipEventReport.event_type;
		handle = data_apdu->message.u.roiv_cmipEventReport.obj_handle;
		time = data_apdu->message.u.roiv_cmipEventReport.event_time;
	} else {
		event = data_apdu->message.u.roiv_cmipConfirmedEventReport.event_info;
		type = data_apdu->message.u.roiv_cmipConfirmedEventReport.event_type;
		handle = data_apdu->message.u.roiv_cmipConfirmedEventReport.obj_handle;
		time = data_apdu->message.u.roiv_cmipConfirmedEventReport.event_time;
	}

	if (handle == 0) {
		if (!operating_decode_mds_event(ctx, type, &event)) {
			data->choice = FSM_EVT_DATA_ERROR_RESULT;
			data->u.error_result.error_value = NO_SUCH_ACTION;
			data->u.error_result.parameter.length = 0;
			data->u.error_result.parameter.value = 0;
			communication_roer_tx(ctx, evt, data);
		}
	} else {
		struct MDS_object *obj = mds_get_object_by_handle(ctx->mds, handle);

		if (obj != NULL && obj->choice == MDS_OBJ_SCANNER) {
			if (obj->u.scanner.choice == EPI_CFG_SCANNER) {
				operating_decode_epi_scan_event(ctx, &obj->u.scanner.u.epi_cfg_scanner, type, &event);
			} else if (obj->u.scanner.choice == PERI_CFG_SCANNER) {
				operating_decode_peri_scan_event(ctx, &obj->u.scanner.u.peri_cfg_scanner, type, &event);
			}
		}
	}

	// If confirmed event report, send the confirmation
	if (data_apdu->message.choice == ROIV_CMIP_CONFIRMED_EVENT_REPORT_CHOSEN) {
		if (data_apdu->message.u.roiv_cmipConfirmedEventReport.event_type == MDC_NOTI_SEGMENT_DATA) {
			// segment data event is always confirmed
			if (!operating_decode_segment_data_event(ctx, data_apdu->invoke_id,
					handle, time, type, &event)) {
				// could not decode due to bad contents
				data->choice = FSM_EVT_DATA_REJECT_RESULT;
				data->u.reject_result.problem = BADLY_STRUCTURED_APDU;
				communication_rorj_tx(ctx, evt, data);
			}
		} else {
			Any event_reply_info;
			memset(&event_reply_info, 0, sizeof(Any));
			event_reply_info.length = 0;
			operating_event_report_response_tx(ctx, data_apdu->invoke_id,
				handle, time, type, event_reply_info);
		}
	}
}

/**
 * Assembles and send event report response.
 *
 * @param ctx
 * @param invoke_id Response invokeID matching event report request.
 * @param obj_handle Object handle.
 * @param currentTime Current system time.
 * @param event_type Event type.
 * @param event_reply_info Event reply data.
 */
void operating_event_report_response_tx(Context *ctx, InvokeIDType invoke_id, ASN1_HANDLE obj_handle,
					RelativeTime currentTime, OID_Type event_type,
					Any event_reply_info)
{
	APDU apdu;
	memset(&apdu, 0, sizeof(APDU));
	apdu.choice = PRST_CHOSEN;

	DATA_apdu data_apdu;
	memset(&data_apdu, 0, sizeof(DATA_apdu));

	data_apdu.invoke_id = invoke_id;
	data_apdu.message.choice = RORS_CMIP_CONFIRMED_EVENT_REPORT_CHOSEN;

	data_apdu.message.length = sizeof(obj_handle) + sizeof(currentTime)
				   + sizeof(event_type) + sizeof(event_reply_info.length)
				   + event_reply_info.length;

	data_apdu.message.u.rors_cmipConfirmedEventReport.obj_handle
	= obj_handle;
	data_apdu.message.u.rors_cmipConfirmedEventReport.currentTime
	= currentTime;
	data_apdu.message.u.rors_cmipConfirmedEventReport.event_type
	= event_type;
	data_apdu.message.u.rors_cmipConfirmedEventReport.event_reply_info
	= event_reply_info;

	apdu.u.prst.length = sizeof(invoke_id)
			     + sizeof(data_apdu.message.choice)
			     + sizeof(data_apdu.message.length)
			     + data_apdu.message.length;

	apdu.length = sizeof(apdu.u.prst.length) + apdu.u.prst.length;
	encode_set_data_apdu(&apdu.u.prst, &data_apdu);
	communication_send_apdu(ctx, &apdu);
}

/**
 * Assembles and send segment data event report response.
 *
 * @param ctx
 * @param invoke_id Response invokeID matching event report request.
 * @param obj_handle Object handle.
 * @param currentTime Current system time.
 * @param event_type Event type.
 * @param result result data.
 */
void operating_segment_data_event_response_tx(Context *ctx, InvokeIDType invoke_id, ASN1_HANDLE obj_handle,
		RelativeTime currentTime, OID_Type event_type, SegmentDataResult result)
{
	ByteStreamWriter *writer;
	writer = byte_stream_writer_instance(sizeof(SegmentDataResult));
	encode_segmentdataresult(writer, &result);

	APDU apdu;
	memset(&apdu, 0, sizeof(APDU));
	apdu.choice = PRST_CHOSEN;

	DATA_apdu data_apdu;
	memset(&data_apdu, 0, sizeof(DATA_apdu));

	data_apdu.invoke_id = invoke_id;
	data_apdu.message.choice = RORS_CMIP_CONFIRMED_EVENT_REPORT_CHOSEN;

	data_apdu.message.length = sizeof(ASN1_HANDLE) + sizeof(RelativeTime)
				   + sizeof(OID_Type) + 2 + writer->size;

	data_apdu.message.u.rors_cmipConfirmedEventReport.obj_handle
	= obj_handle;
	data_apdu.message.u.rors_cmipConfirmedEventReport.currentTime
	= currentTime;
	data_apdu.message.u.rors_cmipConfirmedEventReport.event_type
	= event_type;
	data_apdu.message.u.rors_cmipConfirmedEventReport.event_reply_info.length
	= writer->size;
	data_apdu.message.u.rors_cmipConfirmedEventReport.event_reply_info.value
	= writer->buffer;

	apdu.u.prst.length = sizeof(InvokeIDType)
			     + sizeof(DATA_apdu_choice)
			     + sizeof(intu16)
			     + data_apdu.message.length;

	apdu.length = sizeof(apdu.u.prst.length) + apdu.u.prst.length;

	encode_set_data_apdu(&apdu.u.prst, &data_apdu);
	communication_send_apdu(ctx, &apdu);
	del_byte_stream_writer(writer, 1);
}

/**
 * Agents that have scanner derived objects shall support the SET
 * service for the Operational-State attribute of the scanner objects.
 * This method requires changing the operating state.
 *
 * \param ctx
 * \param handle handle of the scanner object
 * \param state new operational state
 * \param timeout timeout of this action
 * \param callback callback function of the request
 * \return a pointer to a request just done.
 */
Request *operating_set_scanner(Context *ctx, ASN1_HANDLE handle, OperationalState state, intu32 timeout,
			       service_request_callback callback)
{
	APDU *apdu = calloc(1, sizeof(APDU));
	DATA_apdu *data_apdu = calloc(1, sizeof(DATA_apdu));
	AttributeModEntry *entry = calloc(1, sizeof(AttributeModEntry));

	apdu->choice = PRST_CHOSEN;
	data_apdu->message.choice = ROIV_CMIP_CONFIRMED_SET_CHOSEN;
	data_apdu->message.u.roiv_cmipConfirmedSet.obj_handle = handle;
	data_apdu->message.u.roiv_cmipConfirmedSet.modification_list.count = 1;
	data_apdu->message.u.roiv_cmipConfirmedSet.modification_list.value = entry;

	entry[0].modify_operator = 0;
	entry[0].attribute.attribute_id = MDC_ATTR_OP_STAT;
	ByteStreamWriter *stream = byte_stream_writer_instance(sizeof(OperationalState));
	write_intu16(stream, state);
	entry[0].attribute.attribute_value.value = stream->buffer;
	free(stream); // not free stream->buffer

	entry[0].attribute.attribute_value.length = sizeof(OperationalState);
	data_apdu->message.u.roiv_cmipConfirmedSet.modification_list.length
	= sizeof(ModifyOperator) + sizeof(OID_Type) + sizeof(intu16)
	  + entry[0].attribute.attribute_value.length;

	data_apdu->message.length = sizeof(ASN1_HANDLE) + 2 * sizeof(intu16);
	data_apdu->message.length += data_apdu->message.u.roiv_cmipConfirmedSet.modification_list.length;

	apdu->u.prst.length = sizeof(InvokeIDType)
			      + sizeof(data_apdu->message.choice)
			      + sizeof(data_apdu->message.length)
			      + data_apdu->message.length;

	apdu->length = sizeof(apdu->u.prst.length) + apdu->u.prst.length;

	timeout_callback timeout_callback = {.func = &communication_timeout, .timeout = timeout};

	encode_set_data_apdu(&apdu->u.prst, data_apdu);
	return service_send_remote_operation_request(ctx, apdu,
			timeout_callback, callback);
}

/**
 * Updates the state operating with the response sent by the agent
 *
 * @param ctx connection context
 * @param evt input event
 * @param data Contains event data which in this case is the original apdu.
 */
void operating_set_scanner_response(Context *ctx, fsm_events evt, FSMEventData *data)
{
	APDU *apdu = data->received_apdu;
	DATA_apdu *data_apdu = encode_get_data_apdu(&apdu->u.prst);
	SetResultSimple result = data_apdu->message.u.rors_cmipConfirmedSet;

	struct MDS_object *mds_scan_obj = mds_get_object_by_handle(ctx->mds, result.obj_handle);

	if (mds_scan_obj != NULL && mds_scan_obj->choice == MDS_OBJ_SCANNER) {
		struct Scanner_object *scanner = &mds_scan_obj->u.scanner;

		if (result.attribute_list.count > 0) {
			AVA_Type attribute = result.attribute_list.value[0];

			if (attribute.attribute_id == MDC_ATTR_OP_STAT) {

				intu8 *value = attribute.attribute_value.value;
				intu16 length = attribute.attribute_value.length;

				ByteStreamReader *stream = byte_stream_reader_instance(value, length);
				OperationalState state = read_intu16(stream, NULL);
				free(stream);

				switch (scanner->choice) {
				case EPI_CFG_SCANNER: {
					struct EpiCfgScanner *scan = &scanner->u.epi_cfg_scanner;
					epi_cfg_scanner_set_operational_state_response(scan, state);
				}
				break;
				case PERI_CFG_SCANNER: {
					struct PeriCfgScanner *scan = &scanner->u.peri_cfg_scanner;
					peri_cfg_scanner_set_operational_state_response(scan, state);
				}
				break;
				}
			}
		}
	}

}

/**
 * Response of get request
 * @param ctx
 * @param evt
 * @param data
 */
void operating_get_response(Context *ctx, fsm_events evt, FSMEventData *data)
{
	APDU *apdu = data->received_apdu;
	DATA_apdu *data_apdu = encode_get_data_apdu(&apdu->u.prst);

	GetResultSimple *result = &data_apdu->message.u.rors_cmipGet;

	Request *r = service_check_known_invoke_id(ctx, data_apdu);
	if (!r) {
		DEBUG("operating_get_response: no related Request");
		return;
	}

	struct MDS_object *mds_obj = mds_get_object_by_handle(ctx->mds, result->obj_handle);
	int is_msd_pmstore_obj = mds_obj != NULL && mds_obj->choice == MDS_OBJ_PMSTORE;

	int i;

	for (i = 0; i < result->attribute_list.count; i++) {
		AVA_Type *attribute =  &result->attribute_list.value[i];

		if (result->obj_handle == MDS_HANDLE) {
			mds_set_attribute(ctx->mds, attribute);
		} else if (is_msd_pmstore_obj) {
			pmstore_service_set_attribute(&mds_obj->u.pmstore, attribute);
		}
	}

	if (is_msd_pmstore_obj) {
		pmstore_get_data_result(&mds_obj->u.pmstore, &(r->return_data), 0, 0);
	}
}

/**
 * Creates an apdu to execute get service from mds into agent
 *
 * @param ctx
 * @param handle the handle of desire object (0 = MDS)
 * @param attributeids_list list of ids or NULL if you want to get all attributes
 * @param attributeids_list_count number of attribute ids 0 if you want to get all attributes
 * @param timeout
 * @param request_callback
 * @return Request descriptor struct, or NULL if request could not be sent
 */
Request *operating_service_get(Context *ctx, ASN1_HANDLE handle, OID_Type *attributeids_list,
				int attributeids_list_count, intu32 timeout,
				service_request_callback request_callback)
{
#ifndef USE_REQ_MSG
	if (communication_is_trans(ctx)) {
		// FIXME pmstore case
		return service_trans_request(ctx, request_callback);
	}

	APDU *apdu = calloc(1, sizeof(APDU));

	if (apdu != NULL) {
		apdu->choice = PRST_CHOSEN;

		DATA_apdu *data_apdu = calloc(1, sizeof(DATA_apdu));
		data_apdu->message.choice = ROIV_CMIP_GET_CHOSEN;

		GetArgumentSimple arg_simple;
		arg_simple.obj_handle = handle;
		arg_simple.attribute_id_list.count = 0;
		arg_simple.attribute_id_list.length = 0;
		arg_simple.attribute_id_list.value = NULL;

		if (attributeids_list_count > 0 && attributeids_list == NULL) { // Get All
			// iterate and get attribute ids
			arg_simple.attribute_id_list.value = calloc(attributeids_list_count, sizeof(OID_Type));

			if (arg_simple.attribute_id_list.value != NULL) {
				arg_simple.attribute_id_list.count = attributeids_list_count;
				arg_simple.attribute_id_list.length = attributeids_list_count * sizeof(OID_Type);
				arg_simple.attribute_id_list.value = attributeids_list;
			}
		}

		data_apdu->message.u.roiv_cmipGet = arg_simple;

		data_apdu->message.length = arg_simple.attribute_id_list.length;
		data_apdu->message.length += sizeof(ASN1_HANDLE) + sizeof(intu16)
					     + sizeof(intu16);

		apdu->u.prst.length = sizeof(InvokeIDType)
				      + sizeof(DATA_apdu_choice);
		apdu->u.prst.length += data_apdu->message.length
				       + sizeof(intu16);

		apdu->length += sizeof(intu16) + apdu->u.prst.length;

		timeout_callback timeout_callback = { .func = &communication_timeout , .timeout = timeout};
		encode_set_data_apdu(&apdu->u.prst, data_apdu);
		return service_send_remote_operation_request(ctx, apdu,
				timeout_callback, request_callback);
	}

	return NULL;
#endif
}

/**
 * Action for set time
 *
 * @param ctx current context.
 * @param time
 * @param timeout
 * @param request_callback
 * @return the request created
 */
Request *operating_action_set_time(Context *ctx, SetTimeInvoke *time, intu32 timeout,  service_request_callback request_callback)
{
	if (communication_is_trans(ctx)) {
		return service_trans_set_time_request(ctx, time, timeout, request_callback);
	}

	APDU *apdu = (APDU *)calloc(1, sizeof(APDU));

	if (apdu != NULL) {
		apdu->choice = PRST_CHOSEN;

		DATA_apdu *data_apdu = calloc(1, sizeof(DATA_apdu));

		if (data_apdu != NULL) {
			data_apdu->message.choice = ROIV_CMIP_CONFIRMED_ACTION_CHOSEN;

			data_apdu->message.u.roiv_cmipConfirmedAction.obj_handle = 0;
			data_apdu->message.u.roiv_cmipConfirmedAction.action_type
			= MDC_ACT_SET_TIME;

			int length = sizeof(AbsoluteTime) + sizeof(FLOAT_Type);

			ByteStreamWriter *writer = byte_stream_writer_instance(length);
			encode_settimeinvoke(writer, time);

			data_apdu->message.u.roiv_cmipConfirmedAction.action_info_args.value
			= writer->buffer;
			data_apdu->message.u.roiv_cmipConfirmedAction.action_info_args.length
			= writer->size;

			data_apdu->message.length
			= data_apdu->message.u.roiv_cmipConfirmedAction.action_info_args.length
			  + sizeof(intu16)
			  + sizeof(OID_Type)
			  + sizeof(ASN1_HANDLE);

			apdu->u.prst.length = data_apdu->message.length
					      + sizeof(intu16) + sizeof(DATA_apdu_choice)
					      + sizeof(InvokeIDType);


			apdu->length = apdu->u.prst.length + sizeof(intu16);

			// Send APDU
			timeout_callback timeout_callback = { .func = &communication_timeout, .timeout = timeout};
			encode_set_data_apdu(&apdu->u.prst, data_apdu);
			Request *req = service_send_remote_operation_request(ctx, apdu,
					timeout_callback, request_callback);

			del_byte_stream_writer(writer, 0);

			return req;
		}
	}

	return NULL;
}

/**
 * Service listener callback function
 */
void operating_service_listener(Context *ctx, ServiceState new_state)
{
	if (new_state == FINALIZED) {
		disassociating_release_request_normal_tx(ctx, 0, NULL);
	}
}

/**
 *
 * Waits for all requests been retired and send release association request.
 *
 * @param ctx
 * @param evt
 * @param data
 */
void operating_assoc_release_req_tx(Context *ctx,
				    fsm_events evt, FSMEventData *data)
{
	// Shutdown the service engine
	service_finalize(ctx, operating_service_listener);
}

/**
 * Receive remote operation response from agent
 *
 * @param ctx
 * @param evt
 * @param data
 */
void operating_rors_confirmed_action_tx(Context *ctx,
					fsm_events evt, FSMEventData *data)
{
	DATA_apdu *data_apdu = encode_get_data_apdu(
				       &data->received_apdu->u.prst);

	if (data_apdu->message.choice != RORS_CMIP_CONFIRMED_ACTION_CHOSEN) {
		DEBUG("operating_rors_confirmed_action_tx error!");
		return;
	}

	Request *r = service_check_known_invoke_id(ctx, data_apdu);
	if (!r) {
		DEBUG("operating_rors_confirmed_action_tx: no related Request");
		return;
	}

	// Check action type
	if (data_apdu->message.u.rors_cmipConfirmedAction.action_type == MDC_ACT_DATA_REQUEST) {
		DataResponse response;
		int error = 0;

		ByteStreamReader *response_data = byte_stream_reader_instance(data_apdu->message.u.rors_cmipConfirmedAction.action_info_args.value,
						  data_apdu->message.u.rors_cmipConfirmedAction.action_info_args.length);

		decode_dataresponse(response_data, &response, &error);
		free(response_data);

		if (error) {
			return;
		}

		// Send the event to the MDS
		Any event;
		OID_Type type;

		type = response.event_type;
		event.length = response.event_info.length;
		event.value = response.event_info.value;

		// ignore result because it is RORS
		operating_decode_mds_event(ctx, type, &event);


	} else if (data_apdu->message.u.rors_cmipConfirmedAction.action_type == MDC_ACT_SET_TIME) {
		// not needed
	} else if (data_apdu->message.u.rors_cmipConfirmedAction.action_type == MDC_ACT_SEG_CLR) {
		operating_decode_clear_segment(ctx->mds,
				data_apdu->message.u.rors_cmipConfirmedAction.obj_handle,
				r, 0, 0);
	} else if (data_apdu->message.u.rors_cmipConfirmedAction.action_type == MDC_ACT_SEG_GET_INFO) {
		operating_decode_segment_info(ctx->mds,
				&(data_apdu->message.u.rors_cmipConfirmedAction.action_info_args),
				data_apdu->message.u.rors_cmipConfirmedAction.obj_handle,
				r, 0, 0);
	} else if (data_apdu->message.u.rors_cmipConfirmedAction.action_type == MDC_ACT_SEG_TRIG_XFER) {
		operating_decode_trig_segment_data_xfer_response(ctx->mds,
				&(data_apdu->message.u.rors_cmipConfirmedAction.action_info_args),
				data_apdu->message.u.rors_cmipConfirmedAction.obj_handle,
				r, 0, 0);
	}
}

/*
 * Receive remote operation response from agent
 *
 * @param ctx
 * @param evt
 * @param data
 */
void operating_roer_confirmed_action_tx(Context *ctx,
					fsm_events evt, FSMEventData *data)
{
	DATA_apdu *data_apdu = encode_get_data_apdu(
				       &data->received_apdu->u.prst);

	DEBUG("ROER APDU Received");

	if (data_apdu->message.choice != ROER_CHOSEN) {
		DEBUG("roer: error!");
		return;
	}

	int error = data_apdu->message.u.roer.error_value;

	Request *r = service_check_known_invoke_id(ctx, data_apdu);
	if (!r) {
		DEBUG("roer: no related Request");
		return;
	}

	// original request
	int choice = get_roiv_choice(r);
	int action = get_roiv_action_type(r);
	int handle = get_roiv_object_handle(r);

	DEBUG("handling ROER orig %d %d %d", choice, action, handle);

	if (handle < 0) {
		DEBUG("roer: no object handle");
		return;
	}

	if (choice == ROIV_CMIP_GET_CHOSEN) {
		struct MDS_object *mds_obj = mds_get_object_by_handle(ctx->mds, handle);
		int is_msd_pmstore_obj = mds_obj != NULL && mds_obj->choice == MDS_OBJ_PMSTORE;
		if (is_msd_pmstore_obj) {
			pmstore_get_data_result(&mds_obj->u.pmstore, &(r->return_data), 1, error);
		}
	} else if (action == MDC_ACT_SET_TIME) {
		// not needed
	} else if (action == MDC_ACT_SEG_CLR) {
		operating_decode_clear_segment(ctx->mds, handle, r, 1, error);
	} else if (action == MDC_ACT_SEG_GET_INFO) {
		operating_decode_segment_info(ctx->mds, 0, handle, r, 1, error);
	} else if (action == MDC_ACT_SEG_TRIG_XFER) {
		operating_decode_trig_segment_data_xfer_response(ctx->mds, 0, handle, r, 1, error);
	}
}

/*
 * Receive remote operation response from agent
 *
 * @param ctx
 * @param evt
 * @param data
 */
void operating_rorj_confirmed_action_tx(Context *ctx,
					fsm_events evt, FSMEventData *data)
{
	DEBUG("RORJ APDU Received during operating");

	DATA_apdu *data_apdu = encode_get_data_apdu(
				       &data->received_apdu->u.prst);

	if (data_apdu->message.choice != RORJ_CHOSEN) {
		DEBUG("rorj: error!");
		return;
	}
	int error = data_apdu->message.u.rorj.problem;

	Request *r = service_check_known_invoke_id(ctx, data_apdu);
	if (!r) {
		DEBUG("rorj: no related Request");
		return;
	}

	// original request
	int choice = get_roiv_choice(r);
	int action = get_roiv_action_type(r);
	int handle = get_roiv_object_handle(r);

	if (handle < 0) {
		DEBUG("rorj: no object handle");
		return;
	}

	DEBUG("handling RORJ orig %d %d %d", choice, action, handle);

	if (choice == ROIV_CMIP_GET_CHOSEN) {
		struct MDS_object *mds_obj = mds_get_object_by_handle(ctx->mds, handle);
		int is_msd_pmstore_obj = mds_obj != NULL && mds_obj->choice == MDS_OBJ_PMSTORE;
		if (is_msd_pmstore_obj) {
			pmstore_get_data_result(&mds_obj->u.pmstore, &(r->return_data), 1, error);
		}
	} else if (action == MDC_ACT_SET_TIME) {
		// not needed
	} else if (action == MDC_ACT_SEG_CLR) {
		operating_decode_clear_segment(ctx->mds, handle, r, 2, error);
	} else if (action == MDC_ACT_SEG_GET_INFO) {
		operating_decode_segment_info(ctx->mds, 0, handle, r, 2, error);
	} else if (action == MDC_ACT_SEG_TRIG_XFER) {
		operating_decode_trig_segment_data_xfer_response(ctx->mds, 0, handle, r, 2, error);
	}
}

/**
 * Decode incoming PeriCfgScanner event.
 *
 * \param ctx current context.
 * \param scanner a pointer to the scanner object
 * \param event_type the incoming event type
 * \param event the event data
 */
void operating_decode_peri_scan_event(Context *ctx, struct PeriCfgScanner *scanner, OID_Type event_type, Any *event)
{
	int error = 0;

	ScanReportInfoFixed info_fixed;
	ScanReportInfoVar info_var;
	ScanReportInfoGrouped info_grouped;
	ScanReportInfoMPFixed info_mp_fixed;
	ScanReportInfoMPVar info_mp_var;
	ScanReportInfoMPGrouped info_mp_grouped;

	ByteStreamReader *event_info_stream = byte_stream_reader_instance(event->value, event->length);

	DEBUG(" operating: Event Type: %d", event_type);

	switch (event_type) {
	case MDC_NOTI_BUF_SCAN_REPORT_VAR:
		decode_scanreportinfovar(event_info_stream, &info_var, &error);
		if (error)
			break;
		peri_cfg_scanner_event_report_buf_scan_report_var(ctx, scanner, &info_var);
		del_scanreportinfovar(&info_var);
		break;
	case MDC_NOTI_BUF_SCAN_REPORT_FIXED:
		decode_scanreportinfofixed(event_info_stream, &info_fixed, &error);
		if (error)
			break;
		peri_cfg_scanner_event_report_buf_scan_report_fixed(ctx, scanner, &info_fixed);
		del_scanreportinfofixed(&info_fixed);
		break;
	case MDC_NOTI_BUF_SCAN_REPORT_GROUPED:
		decode_scanreportinfogrouped(event_info_stream, &info_grouped, &error);
		if (error)
			break;
		peri_cfg_scanner_event_report_buf_scan_report_grouped(ctx, scanner, &info_grouped);
		del_scanreportinfogrouped(&info_grouped);
		break;
	case MDC_NOTI_BUF_SCAN_REPORT_MP_VAR:
		decode_scanreportinfompvar(event_info_stream, &info_mp_var, &error);
		if (error)
			break;
		peri_cfg_scanner_event_report_buf_scan_report_mp_var(ctx, scanner, &info_mp_var);
		del_scanreportinfompvar(&info_mp_var);
		break;
	case MDC_NOTI_BUF_SCAN_REPORT_MP_FIXED:
		decode_scanreportinfompfixed(event_info_stream, &info_mp_fixed, &error);
		if (error)
			break;
		peri_cfg_scanner_event_report_buf_scan_report_mp_fixed(ctx, scanner, &info_mp_fixed);
		del_scanreportinfompfixed(&info_mp_fixed);
		break;
	case MDC_NOTI_BUF_SCAN_REPORT_MP_GROUPED:
		decode_scanreportinfompgrouped(event_info_stream, &info_mp_grouped, &error);
		if (error)
			break;
		peri_cfg_scanner_event_report_buf_scan_report_mp_grouped(ctx, scanner, &info_mp_grouped);
		del_scanreportinfompgrouped(&info_mp_grouped);
		break;
	}

	free(event_info_stream);
}

/**
 * Decode incoming EpiCfgScanner event.
 *
 * \param ctx
 * \param scanner a pointer to the scanner object
 * \param event_type the incoming event type
 * \param event the event data
 */
void operating_decode_epi_scan_event(Context *ctx, struct EpiCfgScanner *scanner, OID_Type event_type, Any *event)
{
	int error = 0;

	ScanReportInfoFixed info_fixed;
	ScanReportInfoVar info_var;
	ScanReportInfoGrouped info_grouped;
	ScanReportInfoMPFixed info_mp_fixed;
	ScanReportInfoMPVar info_mp_var;
	ScanReportInfoMPGrouped info_mp_grouped;

	ByteStreamReader *event_info_stream = byte_stream_reader_instance(event->value, event->length);

	DEBUG(" operating: Event Type: %d", event_type);

	switch (event_type) {
	case MDC_NOTI_UNBUF_SCAN_REPORT_VAR:
		decode_scanreportinfovar(event_info_stream, &info_var, &error);
		if (error)
			break;
		epi_cfg_scanner_event_report_unbuf_scan_report_var(ctx, scanner, &info_var);
		del_scanreportinfovar(&info_var);
		break;
	case MDC_NOTI_UNBUF_SCAN_REPORT_FIXED:
		decode_scanreportinfofixed(event_info_stream, &info_fixed, &error);
		if (error)
			break;
		epi_cfg_scanner_event_report_unbuf_scan_report_fixed(ctx, scanner, &info_fixed);
		del_scanreportinfofixed(&info_fixed);
		break;
	case MDC_NOTI_UNBUF_SCAN_REPORT_GROUPED:
		decode_scanreportinfogrouped(event_info_stream, &info_grouped, &error);
		if (error)
			break;
		epi_cfg_scanner_event_report_unbuf_scan_report_grouped(ctx, scanner, &info_grouped);
		del_scanreportinfogrouped(&info_grouped);
		break;
	case MDC_NOTI_UNBUF_SCAN_REPORT_MP_VAR:
		decode_scanreportinfompvar(event_info_stream, &info_mp_var, &error);
		if (error)
			break;
		epi_cfg_scanner_event_report_unbuf_scan_report_mp_var(ctx, scanner, &info_mp_var);
		del_scanreportinfompvar(&info_mp_var);
		break;
	case MDC_NOTI_UNBUF_SCAN_REPORT_MP_FIXED:
		decode_scanreportinfompfixed(event_info_stream, &info_mp_fixed, &error);
		if (error)
			break;
		epi_cfg_scanner_event_report_unbuf_scan_report_mp_fixed(ctx, scanner, &info_mp_fixed);
		del_scanreportinfompfixed(&info_mp_fixed);
		break;
	case MDC_NOTI_UNBUF_SCAN_REPORT_MP_GROUPED:
		decode_scanreportinfompgrouped(event_info_stream, &info_mp_grouped, &error);
		if (error)
			break;
		epi_cfg_scanner_event_report_unbuf_scan_report_mp_grouped(ctx, scanner, &info_mp_grouped);
		del_scanreportinfompgrouped(&info_mp_grouped);
		break;
	}

	free(event_info_stream);
}

/**
 * Decode incoming mds event
 *
 * \param ctx
 * \param event_type
 * \param event
 * \return success status
 */
int operating_decode_mds_event(Context *ctx, OID_Type event_type, Any *event)
{
	int error = 0;
	int ret = 1;

	ScanReportInfoFixed info_fixed;
	ScanReportInfoVar info_var;
	ScanReportInfoMPFixed info_mp_fixed;
	ScanReportInfoMPVar info_mp_var;

	ByteStreamReader *event_info_stream = byte_stream_reader_instance(event->value, event->length);

	DEBUG(" operating: Event Type: %d", event_type);

	switch (event_type) {
	case MDC_NOTI_SCAN_REPORT_FIXED:
		decode_scanreportinfofixed(event_info_stream, &info_fixed, &error);
		if (! error) {
			mds_event_report_dynamic_data_update_fixed(ctx, &info_fixed);
		}
		del_scanreportinfofixed(&info_fixed);
		break;
	case MDC_NOTI_SCAN_REPORT_VAR:
		decode_scanreportinfovar(event_info_stream, &info_var, &error);
		if (! error) {
			mds_event_report_dynamic_data_update_var(ctx, &info_var);
		}
		del_scanreportinfovar(&info_var);
		break;
	case MDC_NOTI_SCAN_REPORT_MP_FIXED:
		decode_scanreportinfompfixed(event_info_stream, &info_mp_fixed, &error);
		if (! error) {
			mds_event_report_dynamic_data_update_mp_fixed(ctx, &info_mp_fixed);
		}
		del_scanreportinfompfixed(&info_mp_fixed);
		break;
	case MDC_NOTI_SCAN_REPORT_MP_VAR:
		decode_scanreportinfompvar(event_info_stream, &info_mp_var, &error);
		if (! error) {
			mds_event_report_dynamic_data_update_mp_var(ctx, &info_mp_var);
		}
		del_scanreportinfompvar(&info_mp_var);
		break;
	default:
		ret = 0;
		break;
	}

	free(event_info_stream);

	return ret;
}

/**
 * Decode incoming segment info
 *
 * \param mds the current mds
 * \param obj_handle
 * \param r the related Request
 * \param errtype error type (1=roer, 2=rorj)
 * \param err 11073 error
 */
void operating_decode_clear_segment(struct MDS *mds, ASN1_HANDLE obj_handle, Request *r,
					int errtype, int err)
{
	struct MDS_object *mds_obj;
	mds_obj = mds_get_object_by_handle(mds, obj_handle);

	if (mds_obj->choice == MDS_OBJ_PMSTORE) {
		pmstore_clear_segment_result(&(mds_obj->u.pmstore), r->return_data,
						errtype, err);
	}
}

/**
 * Decode incoming segment info
 *
 * \param mds the current mds
 * \param *event
 * \param *obj_handle
 * \param *r the related Request
 * \param errtype error type (1=roer, 2=rorj)
 * \param err 11073 error
 */
void operating_decode_segment_info(struct MDS *mds, Any *event, ASN1_HANDLE obj_handle, Request *r,
					int errtype, int err)
{
	int error = 0;
	SegmentInfoList info_list;
	struct MDS_object *mds_obj;
	mds_obj = mds_get_object_by_handle(mds, obj_handle);

	if (err)
		goto finally;

	ByteStreamReader *event_info_stream = byte_stream_reader_instance(event->value, event->length);
	decode_segmentinfolist(event_info_stream, &info_list, &error);
	free(event_info_stream);

	if (error) {
		DEBUG("Error decoding segment info");
		return;
	}

finally:
	if (mds_obj->choice == MDS_OBJ_PMSTORE) {
		pmstore_get_segmentinfo_result(&(mds_obj->u.pmstore), info_list, &(r->return_data),
						errtype, err);
	}

	del_segmentinfolist(&info_list);
}

/**
 * Decode incoming segment data transfer response
 *
 * \param mds the current mds
 * \param *event
 * \param *obj_handle
 * \param *r the related Request
 * \param errtype error type (1=roer, 2=rorj)
 * \param err 11073 error
 */
void operating_decode_trig_segment_data_xfer_response(struct MDS *mds, Any *event, ASN1_HANDLE obj_handle,
							Request *r, int errtype, int err)
{
	int error = 0;
	TrigSegmDataXferRsp trig_rsp;
	struct MDS_object *mds_obj;
	mds_obj = mds_get_object_by_handle(mds, obj_handle);

	if (err)
		goto finally;

	ByteStreamReader *event_data_stream = byte_stream_reader_instance(event->value, event->length);
	decode_trigsegmdataxferrsp(event_data_stream, &trig_rsp, &error);
	free(event_data_stream);

	if (error) {
		DEBUG("Error decoding trig segment response");
		return;
	}

finally:
	if (mds_obj->choice == MDS_OBJ_PMSTORE) {
		pmstore_trig_segment_data_xfer_response(&(mds_obj->u.pmstore),
							trig_rsp, &(r->return_data),
							errtype, err);
	}
}

/**
 * Decode incoming segment data event
 *
 * \param ctx
 * \param invoke_id
 * \param obj_handle
 * \param currentTime
 * \param event_type
 * \param event
 * \return success
 */
int operating_decode_segment_data_event(Context *ctx, InvokeIDType invoke_id, ASN1_HANDLE obj_handle,
		RelativeTime currentTime, OID_Type event_type, Any *event)
{
	int error = 0;
	SegmentDataEvent segm_data_event;
	SegmentDataResult result;

	struct MDS_object *mds_obj;
	mds_obj = mds_get_object_by_handle(ctx->mds, obj_handle);

	ByteStreamReader *event_data_stream = byte_stream_reader_instance(event->value, event->length);
	decode_segmentdataevent(event_data_stream, &segm_data_event, &error);
	free(event_data_stream);

	if (error) {
		DEBUG("Error decoding segment data evt");
		return 0;
	}

	result.segm_data_event_descr = segm_data_event.segm_data_event_descr;
	result.segm_data_event_descr.segm_evt_status = SEVTSTA_MANAGER_ABORT;

	if (!(segm_data_event.segm_data_event_descr.segm_evt_status & SEVTSTA_AGENT_ABORT) &&
			mds_obj && mds_obj->choice == MDS_OBJ_PMSTORE) {

		int ok = pmstore_segment_data_event(ctx, &(mds_obj->u.pmstore), segm_data_event);
		if (ok) {
			result.segm_data_event_descr.segm_evt_status = SEVTSTA_MANAGER_CONFIRM;
		}
	}

	del_segmentdataevent(&segm_data_event);

	operating_segment_data_event_response_tx(ctx, invoke_id, obj_handle,
			currentTime, event_type, result);

	return 1;
}
/** @} */
