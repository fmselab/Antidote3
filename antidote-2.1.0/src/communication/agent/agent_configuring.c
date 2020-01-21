/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/**
 * \file configuring.c
 * \brief Configuring module source.
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
 * \author Jose Martins
 * \date Jun 23, 2010
 */

/**
 * \defgroup Configuring Configuring
 * \brief Configuring module.
 * \ingroup FSM
 * @{
 */

#include <stdio.h>
#include <stdlib.h>
#include "src/dim/mds.h"
#include "src/agent_p.h"
#include "src/dim/nomenclature.h"
#include "src/communication/communication.h"
#include "src/communication/agent/agent_configuring.h"
#include "src/communication/agent/agent_association.h"
#include "src/communication/common/service.h"
#include "src/communication/common/stdconfigurations.h"
#include "src/communication/common/extconfigurations.h"
#include "src/communication/agent/agent_disassociating.h"
#include "src/util/bytelib.h"
#include "src/communication/parser/decoder_ASN1.h"
#include "src/communication/parser/encoder_ASN1.h"
#include "src/communication/parser/struct_cleaner.h"
#include "src/util/log.h"

/**
 * Process incoming APDU's in config_sending state (Agent)
 *
 * @param ctx context
 * @param apdu received APDU
 */
void configuring_agent_config_sending_process_apdu(Context *ctx, APDU *apdu)
{
	// agent does not stay in this state for very long
	// same handling as waiting_approval
	configuring_agent_waiting_approval_process_apdu(ctx, apdu);
}

/**
 * Process rors in APDU
 *
 * @param ctx
 * @param *apdu
 */
static void comm_agent_process_confirmed_event_report(Context *ctx, APDU *apdu,
					EventReportResultSimple *report,
					FSMEventData *data)
{
	int error = 0;

	if (report->obj_handle != MDS_HANDLE)
		goto fail;
	if (report->event_type != MDC_NOTI_CONFIG)
		goto fail;

	ConfigReportRsp rsp;
	ByteStreamReader *stream = byte_stream_reader_instance(report->event_reply_info.value,
						report->event_reply_info.length);
	decode_configreportrsp(stream, &rsp, &error);
	free(stream);

	if (error)
		goto fail;

	if (rsp.config_result == ACCEPTED_CONFIG) {
		communication_fire_evt(ctx,
		       fsm_evt_rx_rors_confirmed_event_report_known,
		       data);
	} else {
		communication_fire_evt(ctx,
		       fsm_evt_rx_rors_confirmed_event_report_unknown,
		       data);
	}

	return;
fail:
	communication_fire_evt(ctx, fsm_evt_rx_rors, data);
}

/**
 * Process rors in APDU
 *
 * @param ctx
 * @param *apdu
 */
static void communication_agent_process_rors(Context *ctx, APDU *apdu)
{
	DATA_apdu *data_apdu = encode_get_data_apdu(&apdu->u.prst);
	FSMEventData data;

	if (service_check_known_invoke_id(ctx, data_apdu)) {
		switch (data_apdu->message.choice) {
		case RORS_CMIP_CONFIRMED_EVENT_REPORT_CHOSEN:
			data.received_apdu = apdu;
			comm_agent_process_confirmed_event_report(ctx, apdu,
				&(data_apdu->message.u.rors_cmipConfirmedEventReport),
 				&data);
			break;
		case RORS_CMIP_GET_CHOSEN:
			data.received_apdu = apdu;
			communication_fire_evt(ctx, fsm_evt_rx_rors_get, &data);
			break;
		case RORS_CMIP_CONFIRMED_ACTION_CHOSEN:
			data.received_apdu = apdu;
			communication_fire_evt(ctx,
					       fsm_evt_rx_rors_confirmed_action, &data);
			break;
		case RORS_CMIP_CONFIRMED_SET_CHOSEN:
			data.received_apdu = apdu;
			communication_fire_evt(ctx, fsm_evt_rx_rors_confirmed_set,
					       &data);
			break;
		default:
			break;
		}

		service_request_retired(ctx, data_apdu);
	}
}


/**
 * Process roiv in APDU
 *
 * @param ctx
 * @param *apdu
 */
static void communication_agent_process_roiv(Context *ctx, APDU *apdu)
{
	DATA_apdu *data_apdu = encode_get_data_apdu(&apdu->u.prst);
	FSMEventData data;

	switch (data_apdu->message.choice) {
		case ROIV_CMIP_GET_CHOSEN:
			data.received_apdu = apdu;
			communication_fire_evt(ctx, fsm_evt_rx_roiv_get, &data);
			break;
		default:
			communication_fire_evt(ctx, fsm_evt_rx_roiv, NULL);
			break;
	}
}


/**
 * Process incoming APDU's in waiting_approval state (Agent)
 *
 * @param ctx context
 * @param apdu received APDU
 */
void configuring_agent_waiting_approval_process_apdu(Context *ctx, APDU *apdu)
{
	switch (apdu->choice) {
	case PRST_CHOSEN: {
		DATA_apdu *data_apdu = encode_get_data_apdu(&apdu->u.prst);

		if (communication_is_roiv_type(data_apdu)) {
			communication_agent_process_roiv(ctx, apdu);
		} else if (communication_is_roer_type(data_apdu)) {
			communication_fire_evt(ctx, fsm_evt_rx_roer, NULL);
		} else if (communication_is_rorj_type(data_apdu)) {
			communication_fire_evt(ctx, fsm_evt_rx_rorj, NULL);
		} else if (communication_is_rors_type(data_apdu)) {
			communication_agent_process_rors(ctx, apdu);
		}
	}
	break;
	case AARQ_CHOSEN:
		communication_fire_evt(ctx, fsm_evt_rx_aarq, NULL);
		break;
	case AARE_CHOSEN:
		communication_fire_evt(ctx, fsm_evt_rx_aare, NULL);
		break;
	case RLRQ_CHOSEN: {
		FSMEventData evt;
		evt.choice = FSM_EVT_DATA_RELEASE_RESPONSE_REASON;
		evt.u.release_response_reason = RELEASE_RESPONSE_REASON_NORMAL;
		communication_fire_evt(ctx, fsm_evt_rx_rlrq, &evt);
	}
	break;
	case RLRE_CHOSEN:
		communication_fire_evt(ctx, fsm_evt_rx_rlre, NULL);
		break;
	case ABRT_CHOSEN:
		communication_fire_evt(ctx, fsm_evt_rx_abrt, NULL);
		break;
	default:
		break;
	}
}

/**
 * Sends configuration apdu (agent)
 *
 * @param ctx context
 * @param evt
 * @param event_data
 */
void configuring_send_config_tx(Context *ctx, fsm_events evt,
					FSMEventData *event_data)
{
	APDU *apdu = calloc(1, sizeof(APDU));
	PRST_apdu prst;
	DATA_apdu *data = calloc(1, sizeof(DATA_apdu));
	EventReportArgumentSimple evtrep;
	ConfigReport cfgrep;

	DEBUG("Sending configuration since manager does not know it");

	ConfigObjectList *cfg =
		std_configurations_get_configuration_attributes(
				      		agent_configuration()->config);

	data->invoke_id = 0; // filled by service_* call
	data->message.choice = ROIV_CMIP_CONFIRMED_EVENT_REPORT_CHOSEN;

	evtrep.obj_handle = 0;
	evtrep.event_time = 0xFFFFFFFF;
	evtrep.event_type = MDC_NOTI_CONFIG;

	cfgrep.config_obj_list = *cfg;
	cfgrep.config_report_id = agent_configuration()->config;

	// compensate for config_report
	evtrep.event_info.length = cfg->length + 6; // 80 + 6 = 86 for oximeter

	ByteStreamWriter *cfg_writer = byte_stream_writer_instance(evtrep.event_info.length);
	encode_configreport(cfg_writer, &cfgrep);
	evtrep.event_info.value = cfg_writer->buffer;
	// APDU takes ownership of this buffer, deleted by del_apdu()
	del_byte_stream_writer(cfg_writer, 0);

	data->message.u.roiv_cmipConfirmedEventReport = evtrep;
	data->message.length = evtrep.event_info.length + 10; // 86 + 10 = 96 for oximeter

	// prst = length + DATA_apdu
	// take into account data's invoke id and choice
	prst.length = data->message.length + 6; // 96 + 6 = 102 for oximeter
	encode_set_data_apdu(&prst, data);

	apdu->choice = PRST_CHOSEN;
	apdu->length = prst.length + 2; // 102 + 2 = 104 for oximeter
	apdu->u.prst = prst;

	timeout_callback tm = {.func = &communication_timeout, .timeout = 3};

	// takes ownership of apdu and prst.value
	service_send_remote_operation_request(ctx, apdu, tm, NULL);

	del_configobjectlist(cfg);
	free(cfg);
}

/** @} */
