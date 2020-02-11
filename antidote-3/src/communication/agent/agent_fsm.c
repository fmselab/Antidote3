/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/**
 * \file fsm.c
 * \brief Finite State Machine source.
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
 * \author Fabricio Silva, Jose Martins
 * \date Jun 21, 2010
 */


/**
 * \defgroup FSM FSM
 * \brief Finite State Machine
 *
 * An overview of the manager state machine is shown in the below figure.
 * \image html fsm.png
 *
 * @{
 */

#include <stdlib.h>
#include <stdio.h>
#include "src/communication/agent/agent_fsm.h"
#include "src/communication/common/fsm.h"
#include "src/communication/common/disassociating.h"
#include "src/communication/agent/agent_association.h"
#include "src/communication/common/communication.h"
#include "src/communication/agent/agent_disassociating.h"
#include "src/communication/agent/agent_configuring.h"
#include "src/communication/agent/agent_operating.h"
#include "src/communication/agent/agent_ops.h"
#include "src/util/log.h"


/**
 * IEEE 11073 Agent State Table
 */
static FsmTransitionRule IEEE11073_20601_agent_state_table[] = {
	// current			input							next				post_action
	{fsm_state_disconnected,	fsm_evt_ind_transport_connection,			fsm_state_unassociated,		&association_agent_mds}, // 1.1
	{fsm_state_unassociated,	fsm_evt_ind_transport_disconnect,			fsm_state_disconnected,		&communication_disconnect_tx}, // 2.2
	{fsm_state_unassociated,	fsm_evt_req_assoc,					fsm_state_associating,		&association_aarq_tx}, // 2.5
	{fsm_state_unassociated,	fsm_evt_req_assoc_rel,					fsm_state_unassociated,		NULL}, // 2.6
	{fsm_state_unassociated,	fsm_evt_req_assoc_abort,				fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 2.7
	{fsm_state_unassociated,	fsm_evt_rx_aarq,					fsm_state_unassociated,		&association_agent_aare_rejected_permanent_tx}, // 2.8
	{fsm_state_unassociated,	fsm_evt_rx_aare,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 2.12
	{fsm_state_unassociated,	fsm_evt_rx_rlrq,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 2.16
	{fsm_state_unassociated,	fsm_evt_rx_rlre,					fsm_state_unassociated,		NULL}, // 2.17
	{fsm_state_unassociated,	fsm_evt_rx_abrt,					fsm_state_unassociated,		NULL}, // 2.18
	{fsm_state_unassociated,	fsm_evt_rx_prst,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 2.19
	{fsm_state_associating,		fsm_evt_ind_transport_disconnect,			fsm_state_disconnected,		NULL}, // 3.2
	{fsm_state_associating,		fsm_evt_ind_timeout,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 3.4
	{fsm_state_associating,		fsm_evt_req_assoc_rel,					fsm_state_unassociated,		&disassociating_release_request_normal_tx}, // 3.6
	{fsm_state_associating,		fsm_evt_req_assoc_abort,				fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 3.7
	{fsm_state_associating,		fsm_evt_rx_aarq,					fsm_state_unassociated,		&association_agent_aare_rejected_permanent_tx}, // 3.8
	{fsm_state_associating,		fsm_evt_rx_aare_accepted_known,				fsm_state_operating,		NULL}, // 3.13
	{fsm_state_associating,		fsm_evt_rx_aare_accepted_unknown,			fsm_state_config_sending,	NULL}, // 3.14
	{fsm_state_associating,		fsm_evt_rx_aare_rejected,				fsm_state_unassociated,		NULL}, // 3.15
	{fsm_state_associating,		fsm_evt_rx_rlrq,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 3.16
	{fsm_state_associating,		fsm_evt_rx_rlre,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 3.17
	{fsm_state_associating,		fsm_evt_rx_abrt,					fsm_state_unassociated,		NULL}, // 3.18
	{fsm_state_associating,		fsm_evt_rx_prst,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 3.19
	{fsm_state_config_sending,	fsm_evt_ind_transport_disconnect,			fsm_state_disconnected,		NULL}, // 4.2
	{fsm_state_config_sending,	fsm_evt_ind_timeout,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 4.4
	{fsm_state_config_sending,	fsm_evt_req_assoc_rel,					fsm_state_disassociating,	&disassociating_release_request_normal_tx}, // 4.6
	{fsm_state_config_sending,	fsm_evt_req_assoc_abort,				fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 4.7
	{fsm_state_config_sending,	fsm_evt_rx_aarq,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 4.8
	{fsm_state_config_sending,	fsm_evt_rx_aare,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 4.12
	{fsm_state_config_sending,	fsm_evt_rx_rlrq,					fsm_state_unassociated,		&disassociating_release_response_tx_normal}, // 4.16
	{fsm_state_config_sending,	fsm_evt_rx_rlre,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 4.17
	{fsm_state_config_sending,	fsm_evt_rx_abrt,					fsm_state_unassociated,		NULL}, // 4.18
	{fsm_state_config_sending,	fsm_evt_rx_roiv_get,					fsm_state_config_sending,	&communication_agent_roiv_get_mds_tx}, // 4.22
	{fsm_state_config_sending,	fsm_evt_rx_roiv,					fsm_state_config_sending,	&communication_agent_roer_no_tx}, // 4.23
	{fsm_state_config_sending,	fsm_evt_rx_roiv_event_report,				fsm_state_config_sending,	&communication_agent_roer_no_tx}, // 4.23
	{fsm_state_config_sending,	fsm_evt_rx_roiv_confirmed_event_report,			fsm_state_config_sending,	&communication_agent_roer_no_tx}, // 4.23
	{fsm_state_config_sending,	fsm_evt_rx_roiv_set,					fsm_state_config_sending,	&communication_agent_roer_no_tx}, // 4.23
	{fsm_state_config_sending,	fsm_evt_rx_roiv_confirmed_set,				fsm_state_config_sending,	&communication_agent_roer_no_tx}, // 4.23
	{fsm_state_config_sending,	fsm_evt_rx_roiv_action,					fsm_state_config_sending,	&communication_agent_roer_no_tx}, // 4.23
	{fsm_state_config_sending,	fsm_evt_rx_roiv_confirmed_action,			fsm_state_config_sending,	&communication_agent_roer_no_tx}, // 4.23
	{fsm_state_config_sending,	fsm_evt_rx_rors,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 4.26
	{fsm_state_config_sending,	fsm_evt_rx_rors_confirmed_event_report,			fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 4.26
	{fsm_state_config_sending,	fsm_evt_rx_rors_confirmed_event_report_unknown,		fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 4.26
	{fsm_state_config_sending,	fsm_evt_rx_rors_confirmed_event_report_known,		fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 4.26
	{fsm_state_config_sending,	fsm_evt_rx_rors_get,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 4.26
	{fsm_state_config_sending,	fsm_evt_rx_rors_confirmed_set,				fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 4.26
	{fsm_state_config_sending,	fsm_evt_rx_rors_confirmed_action,			fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 4.26
	{fsm_state_config_sending,	fsm_evt_rx_roer,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 4.26
	{fsm_state_config_sending,	fsm_evt_rx_rorj,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 4.26
	{fsm_state_config_sending,	fsm_evt_req_send_config,				fsm_state_waiting_approval,	&configuring_send_config_tx}, // 4.32
	{fsm_state_waiting_approval,	fsm_evt_ind_transport_disconnect,			fsm_state_disconnected,		NULL}, // 5.2
	{fsm_state_waiting_approval,	fsm_evt_ind_timeout,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 5.4
	{fsm_state_waiting_approval,	fsm_evt_req_assoc_rel,					fsm_state_disassociating,	&disassociating_release_request_normal_tx}, // 5.6
	{fsm_state_waiting_approval,	fsm_evt_req_assoc_abort,				fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 5.7
	{fsm_state_waiting_approval,	fsm_evt_rx_aarq,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 5.8
	{fsm_state_waiting_approval,	fsm_evt_rx_aare,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 5.12
	{fsm_state_waiting_approval,	fsm_evt_rx_rlrq,					fsm_state_unassociated,		&disassociating_release_response_tx_normal}, // 5.16
	{fsm_state_waiting_approval,	fsm_evt_rx_rlre,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 5.17
	{fsm_state_waiting_approval,	fsm_evt_rx_abrt,					fsm_state_unassociated,		NULL}, // 5.18
	{fsm_state_waiting_approval,	fsm_evt_rx_roiv_get,					fsm_state_config_sending,	&communication_agent_roiv_get_mds_tx}, // 5.22
	{fsm_state_waiting_approval,	fsm_evt_rx_roiv,					fsm_state_config_sending,	&communication_agent_roer_no_tx}, // 5.23
	{fsm_state_waiting_approval,	fsm_evt_rx_roiv_event_report,				fsm_state_config_sending,	&communication_agent_roer_no_tx}, // 5.23
	{fsm_state_waiting_approval,	fsm_evt_rx_roiv_confirmed_event_report,			fsm_state_config_sending,	&communication_agent_roer_no_tx}, // 5.23
	{fsm_state_waiting_approval,	fsm_evt_rx_roiv_set,					fsm_state_config_sending,	&communication_agent_roer_no_tx}, // 5.23
	{fsm_state_waiting_approval,	fsm_evt_rx_roiv_confirmed_set,				fsm_state_config_sending,	&communication_agent_roer_no_tx}, // 5.23
	{fsm_state_waiting_approval,	fsm_evt_rx_roiv_action,					fsm_state_config_sending,	&communication_agent_roer_no_tx}, // 5.23
	{fsm_state_waiting_approval,	fsm_evt_rx_roiv_confirmed_action,			fsm_state_config_sending,	&communication_agent_roer_no_tx}, // 5.23
	{fsm_state_waiting_approval,	fsm_evt_rx_rors_confirmed_event_report_unknown,		fsm_state_config_sending,	NULL}, // 5.27
	{fsm_state_waiting_approval,	fsm_evt_rx_rors_confirmed_event_report_known,		fsm_state_operating,		NULL}, // 5.29
	{fsm_state_waiting_approval,	fsm_evt_rx_rors,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 5.30
	{fsm_state_waiting_approval,	fsm_evt_rx_rors_confirmed_event_report,			fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 5.30
	{fsm_state_waiting_approval,	fsm_evt_rx_rors_get,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 5.30
	{fsm_state_waiting_approval,	fsm_evt_rx_rors_confirmed_set,				fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 5.30
	{fsm_state_waiting_approval,	fsm_evt_rx_rors_confirmed_action,			fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 5.30
	{fsm_state_waiting_approval,	fsm_evt_rx_roer,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 5.30
	{fsm_state_waiting_approval,	fsm_evt_rx_rorj,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 5.30
	{fsm_state_operating,		fsm_evt_ind_transport_disconnect,			fsm_state_disconnected,		NULL}, // 8.2
	{fsm_state_operating,		fsm_evt_ind_timeout,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 8.4
	{fsm_state_operating,		fsm_evt_req_assoc_rel,					fsm_state_disassociating,	&disassociating_release_request_normal_tx}, // 8.6

	{fsm_state_operating,		fsm_evt_req_assoc_abort,				fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 8.7
	{fsm_state_operating,		fsm_evt_req_send_event,					fsm_state_operating,		&communication_agent_send_event_tx}, // 8.7
	{fsm_state_operating,		fsm_evt_rx_aarq,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 8.8
	{fsm_state_operating,		fsm_evt_rx_aare,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 8.12
	{fsm_state_operating,		fsm_evt_rx_rlrq,					fsm_state_unassociated,		&disassociating_release_response_tx_normal}, // 8.16
	{fsm_state_operating,		fsm_evt_rx_rlre,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 8.17
	{fsm_state_operating,		fsm_evt_rx_abrt,					fsm_state_unassociated,		NULL}, // 8.18
	{fsm_state_operating,		fsm_evt_rx_roiv,					fsm_state_operating,		&communication_agent_roiv_respond_tx}, // 8.21
	{fsm_state_operating,		fsm_evt_rx_roiv_confirmed_event_report,			fsm_state_operating,		&communication_agent_roiv_confirmed_event_report_respond_tx}, // 8.21
	{fsm_state_operating,		fsm_evt_rx_roiv_get,					fsm_state_operating,		&communication_agent_roiv_get_mds_tx}, // 8.21
	{fsm_state_operating,		fsm_evt_rx_roiv_set,					fsm_state_operating,		&communication_agent_roiv_set_respond_tx}, // 8.21
	{fsm_state_operating,		fsm_evt_rx_roiv_confirmed_set,				fsm_state_operating,		&communication_agent_roiv_confirmed_set_respond_tx}, // 8.21
	{fsm_state_operating,		fsm_evt_rx_roiv_confirmed_action,			fsm_state_operating,		&communication_agent_roiv_confirmed_action_respond_tx}, // 8.21
	{fsm_state_operating,		fsm_evt_rx_roiv_action,					fsm_state_operating,		&communication_agent_roiv_action_respond_tx}, // 8.21
	{fsm_state_operating,		fsm_evt_rx_rors,					fsm_state_operating,		NULL}, // 8.26
	{fsm_state_operating,		fsm_evt_rx_rors_confirmed_event_report,			fsm_state_operating,		NULL}, // 8.26
	{fsm_state_operating,		fsm_evt_rx_rors_confirmed_event_report_unknown,		fsm_state_operating,		NULL}, // 8.26
	{fsm_state_operating,		fsm_evt_rx_rors_confirmed_event_report_known,		fsm_state_operating,		NULL}, // 8.26
	{fsm_state_operating,		fsm_evt_rx_rors_get,					fsm_state_operating,		NULL}, // 8.26
	{fsm_state_operating,		fsm_evt_rx_rors_confirmed_set,				fsm_state_operating,		NULL}, // 8.26
	{fsm_state_operating,		fsm_evt_rx_rors_confirmed_action,			fsm_state_operating,		NULL}, // 8.26
	{fsm_state_operating,		fsm_evt_rx_roer,					fsm_state_operating,		NULL}, // 8.26
	{fsm_state_operating,		fsm_evt_rx_rorj,					fsm_state_operating,		NULL}, // 8.26
	{fsm_state_disassociating,	fsm_evt_ind_transport_disconnect,			fsm_state_disconnected,		NULL}, // 9.2
	{fsm_state_disassociating,	fsm_evt_ind_timeout,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 9.4
	{fsm_state_disassociating,	fsm_evt_req_assoc_rel,					fsm_state_disassociating,	NULL}, // 9.6
	{fsm_state_disassociating,	fsm_evt_req_assoc_abort,				fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 9.7
	{fsm_state_disassociating,	fsm_evt_rx_aarq,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 9.8
	{fsm_state_disassociating,	fsm_evt_rx_aare,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 9.12
	{fsm_state_disassociating,	fsm_evt_rx_rlrq,					fsm_state_disassociating,	&disassociating_release_response_tx_normal}, // 9.16
	{fsm_state_disassociating,	fsm_evt_rx_rlre,					fsm_state_unassociated,		NULL}, // 9.17
	{fsm_state_disassociating,	fsm_evt_rx_abrt,					fsm_state_unassociated,		NULL}, // 9.18
	{fsm_state_disassociating,	fsm_evt_rx_roiv,					fsm_state_disassociating,	NULL}, // 9.21
	{fsm_state_disassociating,	fsm_evt_rx_rors,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 9.26
	{fsm_state_disassociating,	fsm_evt_rx_rors_confirmed_event_report,			fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 9.26
	{fsm_state_disassociating,	fsm_evt_rx_rors_confirmed_event_report_unknown,		fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 9.26
	{fsm_state_disassociating,	fsm_evt_rx_rors_confirmed_event_report_known,		fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 9.26
	{fsm_state_disassociating,	fsm_evt_rx_rors_get,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 9.26
	{fsm_state_disassociating,	fsm_evt_rx_rors_confirmed_set,				fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 9.26
	{fsm_state_disassociating,	fsm_evt_rx_rors_confirmed_action,			fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 9.26
	{fsm_state_disassociating,	fsm_evt_rx_roer,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 9.26
	{fsm_state_disassociating,	fsm_evt_rx_rorj,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 9.26
	};

/**
 * Initialize fsm with the states and transition rules of
 * IEEE 11073-20601 for Agent
 *
 * @param fsm
 */
void fsm_set_agent_state_table(FSM *fsm)
{
	int transition_table_size = sizeof(IEEE11073_20601_agent_state_table);
	int trasition_rule_size = sizeof(FsmTransitionRule);
	int table_size = transition_table_size / trasition_rule_size;
	fsm_init(fsm, fsm_state_disconnected, IEEE11073_20601_agent_state_table,
								table_size);
}
/** @} */
