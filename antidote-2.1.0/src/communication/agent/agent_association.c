/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/**
 * \file association.c
 * \brief Association module source.
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
 * \author Fabricio Silva
 * \date Jun 23, 2010
 */

/**
 * \defgroup Association Association
 * \brief Association module.
 * \ingroup FSM
 * @{
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agent_association.h"
#include "src/agent_p.h"
#include "src/communication/communication.h"
#include "src/communication/parser/decoder_ASN1.h"
#include "src/communication/parser/encoder_ASN1.h"
#include "src/util/bytelib.h"
#include "src/communication/parser/struct_cleaner.h"
#include "src/dim/mds.h"
#include "src/util/log.h"


/**
 * Retry count association constant
 */
// static const intu32 ASSOCIATION_RC = 3;

/**
 * Time out (seconds) association constant
 */
// static const intu32 ASSOCIATION_TO = 10;

static int association_check_config_id(PhdAssociationInformation *info);

static void association_process_aarq_apdu(Context *ctx, APDU *apdu);

static void association_process_aare_apdu(Context *ctx, APDU *apdu);

static void association_accept_data_protocol_20601(Context *ctx, DataProto *proto);

static void populate_aare(APDU *apdu, PhdAssociationInformation *response_info);

/**
 * Internal function to populate the AARE with default response data.
 *
 * @param apdu
 * @param response_info
 */
static void populate_aare(APDU *apdu, PhdAssociationInformation *response_info)
{
	apdu->choice = AARE_CHOSEN;
	apdu->length = 44;

	apdu->u.aare.selected_data_proto.data_proto_id = DATA_PROTO_ID_20601;
	apdu->u.aare.selected_data_proto.data_proto_info.length = 38;

	response_info->protocolVersion = ASSOC_VERSION1;
	response_info->encodingRules = MDER;
	response_info->nomenclatureVersion = NOM_VERSION1;

	response_info->functionalUnits = 0x00000000;
	response_info->systemType = SYS_TYPE_MANAGER;

	response_info->system_id.value = manager_system_id();
	response_info->system_id.length = manager_system_id_length();

	response_info->dev_config_id = MANAGER_CONFIG_RESPONSE;
	response_info->data_req_mode_capab.data_req_mode_flags = 0x0000;
	response_info->data_req_mode_capab.data_req_init_agent_count = 0x00;
	response_info->data_req_mode_capab.data_req_init_manager_count = 0x00;

	response_info->optionList.count = 0;
	response_info->optionList.length = 0;
}

/**
 * Process incoming APDU's in unassociated state - agent
 *
 * @param ctx the current context.
 * @param apdu received APDU.
 */
void association_unassociated_process_apdu_agent(Context *ctx, APDU *apdu)
{
	switch (apdu->choice) {
	case AARQ_CHOSEN:
		communication_fire_evt(ctx, fsm_evt_rx_aarq, NULL);
		break;
	case AARE_CHOSEN:
		association_process_aare_apdu(ctx, apdu);
		break;
	case RLRQ_CHOSEN:
		communication_fire_evt(ctx, fsm_evt_rx_rlrq, NULL);
		break;
	case PRST_CHOSEN:
		communication_fire_evt(ctx, fsm_evt_rx_prst, NULL);
		break;
	case ABRT_CHOSEN: // ignore
		break;
	case RLRE_CHOSEN: // ignore
		break;
	default:
		// ignore
		break;
	}
}

/**
 * Process the association response apdu
 *
 * @param apdu
 */
static void association_process_aare_apdu(Context *ctx, APDU *apdu)
{
	DEBUG(" associating: processing aare ");

	if (!apdu)
		return;

	if (apdu->u.aare.result == ACCEPTED) {
		communication_fire_evt(ctx, fsm_evt_rx_aare_accepted_known, NULL);
	} else if (apdu->u.aare.result == ACCEPTED_UNKNOWN_CONFIG) {
		communication_fire_evt(ctx, fsm_evt_rx_aare_accepted_unknown, NULL);
	} else {
		DEBUG("associating: aare rejection code %d", apdu->u.aare.result);
		communication_fire_evt(ctx, fsm_evt_rx_aare_rejected, NULL);
	}
}


static void populate_aarq(APDU *apdu, PhdAssociationInformation *config_info,
				DataProto *proto);

/**
 * Send apdu association request (normally, Agent does this)
 *
 * @param ctx connection context
 * @param evt input event
 * @param data fsm data, unused
 *
 */
void association_aarq_tx(FSMContext *ctx, fsm_events evt, FSMEventData *data)
{
	APDU config_apdu;
	PhdAssociationInformation config_info;
	DataProto proto;

	memset(&config_info, 0, sizeof(PhdAssociationInformation));
	populate_aarq(&config_apdu, &config_info, &proto);

	// Encode APDU
	ByteStreamWriter *encoded_value =
		byte_stream_writer_instance(proto.data_proto_info.length);
		encode_phdassociationinformation(encoded_value, &config_info);

	proto.data_proto_info.value = encoded_value->buffer;

	communication_send_apdu(ctx, &config_apdu);

	del_byte_stream_writer(encoded_value, 1);
	del_phdassociationinformation(&config_info);
}

/**
 * Populate AARQ APDU (Normally, Agent uses this)
 *
 * @param apdu APDU structure
 * @param config_info Configuration to send
 * @param proto Data protocol to send
 */
static void populate_aarq(APDU *apdu, PhdAssociationInformation *config_info,
				DataProto *proto)
{
	struct mds_system_data *mds_data = agent_configuration()->mds_data_cb();

	apdu->choice = AARQ_CHOSEN;
	apdu->length = 50;

	apdu->u.aarq.assoc_version = ASSOC_VERSION1;
	apdu->u.aarq.data_proto_list.count = 1;
	apdu->u.aarq.data_proto_list.length = 42;
	apdu->u.aarq.data_proto_list.value = proto;

	proto->data_proto_id = DATA_PROTO_ID_20601;
	proto->data_proto_info.length = 38;

	config_info->protocolVersion = ASSOC_VERSION1;
	config_info->encodingRules = MDER;
	config_info->nomenclatureVersion = NOM_VERSION1;

	config_info->functionalUnits = 0x00000000;
	config_info->systemType = SYS_TYPE_AGENT;

	config_info->system_id.length = 8;
	config_info->system_id.value = malloc(config_info->system_id.length);
	memcpy(config_info->system_id.value, mds_data->system_id,
					config_info->system_id.length);

	config_info->dev_config_id = agent_configuration()->config;

	config_info->data_req_mode_capab.data_req_mode_flags = DATA_REQ_SUPP_INIT_AGENT;
	// max number of simultaneous sessions
	config_info->data_req_mode_capab.data_req_init_agent_count = 0x01;
	config_info->data_req_mode_capab.data_req_init_manager_count = 0x00;

	config_info->optionList.count = 0;
	config_info->optionList.length = 0;

	free(mds_data);
}


/**
 * Send apdu association rejected (normally, Agent does this if Manager tries to associate)
 *
 * @param ctx connection context
 * @param evt input event
 * @param data fsm data, unused
 */
void association_agent_aare_rejected_permanent_tx(FSMContext *ctx, fsm_events evt, FSMEventData *data)
{
	APDU response_apdu;
	PhdAssociationInformation response_info;

	memset(&response_info, 0, sizeof(PhdAssociationInformation));
	populate_aare(&response_apdu, &response_info);

	response_apdu.u.aare.result = REJECTED_PERMANENT;

	// Encode APDU
	ByteStreamWriter *encoded_value =
		byte_stream_writer_instance(
			response_apdu.u.aare.selected_data_proto .data_proto_info.length);

	encode_phdassociationinformation(encoded_value, &response_info);

	response_apdu.u.aare.selected_data_proto.data_proto_info.value =
		encoded_value->buffer;

	communication_send_apdu(ctx, &response_apdu);

	del_byte_stream_writer(encoded_value, 1);
	del_phdassociationinformation(&response_info);
}

/** @} */
