/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/**
 * \file disassociating.c
 * \brief Disassociating module source.
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
 * \date Jun 30, 2010
 */

/**
 * \defgroup Disassociating Disassociating
 * \brief Disassociating module.
 * \ingroup FSM
 * @{
 */

#include <stdio.h>
#include <stdlib.h>
#include "src/communication/common/disassociating.h"
#include "src/communication/communication.h"
#include "src/communication/parser/decoder_ASN1.h"
#include "src/communication/parser/encoder_ASN1.h"
#include "src/util/bytelib.h"
#include "src/util/log.h"

/**
 * Time out (seconds) association release constant
 */
static const intu32 ASSOCIATION_TO_RELEASE = 10;

/**
 *  Listen to fsm event and send Release Response APDU - normal release
 *
 * @param ctx context
 * @param evt input event
 * @param data (ignored; refilled with NORMAL reason)
 */
void disassociating_release_request_normal_tx(Context *ctx, fsm_events evt,
						FSMEventData *data)
{
	APDU apdu;
	apdu.choice = RLRQ_CHOSEN;
	apdu.length = sizeof(RLRQ_apdu);
	apdu.u.rlrq.reason = RELEASE_REQUEST_REASON_NORMAL;

	DEBUG("releasing association request normal tx (normal) - APDU <%d>", apdu.choice);

	if (communication_send_apdu(ctx, &apdu)) {
		communication_count_timeout(ctx, &communication_timeout,
						ASSOCIATION_TO_RELEASE);
	}
}

/**
 *  Listen to fsm event and send Release Response APDU
 *
 * @param ctx context
 * @param evt input event
 * @param dummy Dummy event data
 */
void disassociating_release_response_tx_normal(Context *ctx, fsm_events evt,
					FSMEventData *dummy)
{
	FSMEventData data;
	data.choice = FSM_EVT_DATA_RELEASE_RESPONSE_REASON;
	data.u.release_response_reason = RELEASE_RESPONSE_REASON_NORMAL;

	DEBUG("releasing association response normal tx(normal)");

	disassociating_release_response_tx(ctx, evt, &data);
}

/** @} */
