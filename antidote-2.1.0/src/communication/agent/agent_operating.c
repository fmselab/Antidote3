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
#include "src/communication/common/service.h"
#include "src/communication/communication.h"
#include "src/communication/agent/agent_operating.h"
#include "src/communication/agent/agent_disassociating.h"
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
#include "src/communication/communication.h"

static void communication_agent_process_roiv(Context *ctx, APDU *apdu);

static void communication_agent_process_rors(Context *ctx, APDU *apdu);

/**
 * Process incoming APDU - Agent
 *
 * @param ctx The context of the Apdu
 * @param apdu The apdu to be processed
 */
void operating_process_apdu_agent(Context *ctx, APDU *apdu)
{
	//RejectResult reject_result;

	DEBUG("processing apdu in operating mode - agent");

	switch (apdu->choice) {
	case PRST_CHOSEN: {
		DATA_apdu *data_apdu = encode_get_data_apdu(&apdu->u.prst);

		if (communication_is_roiv_type(data_apdu)) {
			communication_agent_process_roiv(ctx, apdu);
		} else if (communication_is_rors_type(data_apdu)) {
			communication_agent_process_rors(ctx, apdu);
		}
		break;
	}
	case ABRT_CHOSEN: {
		communication_fire_evt(ctx, fsm_evt_rx_abrt, NULL);
		break;
	}
	case RLRQ_CHOSEN: {
		communication_fire_evt(ctx, fsm_evt_rx_rlrq, NULL);
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
 * Process rors in APDU
 *
 * @param ctx
 * @param *apdu
 */
static void communication_agent_process_rors(Context *ctx, APDU *apdu)
{
	DATA_apdu *data_apdu = encode_get_data_apdu(&apdu->u.prst);
	FSMEventData data;
	//RejectResult reject_result;

	if (service_check_known_invoke_id(ctx, data_apdu)) {
		switch (data_apdu->message.choice) {
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
			DEBUG("Received RORS with invalid choice %d",
						data_apdu->message.choice);
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

/** @} */
