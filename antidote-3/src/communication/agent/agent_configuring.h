/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/**
 * \file configuring.h
 * \brief Configuring module header.
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
 * \ingroup FSM
 * @{
 */

#ifndef CONFIGURING_AGENT_H_
#define CONFIGURING_AGENT_H_

#include "src/asn1/phd_types.h"
#include "src/communication/common/communication.h"

void configuring_agent_config_sending_process_apdu(Context *ctx, APDU *apdu);

void configuring_agent_waiting_approval_process_apdu(Context *ctx, APDU *apdu);

void configuring_send_config_tx(Context *ctx, fsm_events evt,
					FSMEventData *event_data);

/** @} */
#endif /* CONFIGURING_AGENT_H_ */

