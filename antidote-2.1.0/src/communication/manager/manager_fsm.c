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
#include "src/communication/common/fsm.h"
#include "src/communication/manager/manager_fsm.h"
#include "src/communication/manager/manager_association.h"
#include "src/communication/common/communication.h"
#include "src/communication/manager/manager_disassociating.h"
#include "src/communication/manager/manager_configuring.h"
#include "src/communication/manager/manager_operating.h"
#include "src/util/log.h"


/**
 * IEEE 11073 Manager State Table
 */
static FsmTransitionRule IEEE11073_20601_manager_state_table[] = {
	{fsm_state_disconnected,	fsm_evt_ind_transport_connection,				fsm_state_unassociated,		NULL}, // 1.1
	{fsm_state_unassociated,	fsm_evt_ind_transport_disconnect,				fsm_state_disconnected,		&communication_disconnect_tx}, // 2.2
	{fsm_state_unassociated,	fsm_evt_req_assoc_rel,						fsm_state_unassociated,		NULL}, // 2.6
	//AB: Modificata la risposta {fsm_state_unassociated,	fsm_evt_req_assoc_abort,					fsm_state_unassociated,		NULL}, // 2.7
	{fsm_state_unassociated,	fsm_evt_req_assoc_abort,					fsm_state_unassociated,         &communication_abort_undefined_reason_tx},
	{fsm_state_unassociated,	fsm_evt_rx_aarq_acceptable_and_known_configuration,		fsm_state_operating,		&association_accept_config_tx}, // 2.9
	{fsm_state_unassociated,	fsm_evt_rx_aarq_acceptable_and_unknown_configuration,		fsm_state_waiting_for_config,	&configuring_transition_waiting_for_config}, // 2.10

	{fsm_state_unassociated,	fsm_evt_rx_aarq_unacceptable_configuration,			fsm_state_unassociated,		&association_unaccept_config_tx}, // 2.11


	{fsm_state_unassociated,	fsm_evt_rx_aare,						fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 2.12
	{fsm_state_unassociated,	fsm_evt_rx_rlrq,						fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 2.16
	{fsm_state_unassociated,	fsm_evt_rx_rlre,						fsm_state_unassociated,		NULL}, // 2.17
	{fsm_state_unassociated,	fsm_evt_rx_abrt,						fsm_state_unassociated,		NULL}, // 2.18
	{fsm_state_unassociated,	fsm_evt_rx_prst,						fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 2.19


	{fsm_state_waiting_for_config,	fsm_evt_ind_transport_disconnect,				fsm_state_disconnected,		&communication_disconnect_tx}, // 6.2
	{fsm_state_waiting_for_config,	fsm_evt_ind_timeout,						fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 6.4
	{fsm_state_waiting_for_config,	fsm_evt_req_assoc_rel,						fsm_state_disassociating,	&configuring_association_release_request_tx}, // 6.6
	{fsm_state_waiting_for_config,	fsm_evt_req_assoc_abort,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 6.7
	{fsm_state_waiting_for_config,	fsm_evt_rx_aarq,						fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 6.8
	{fsm_state_waiting_for_config,	fsm_evt_rx_aare,						fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 6.12
	{fsm_state_waiting_for_config,	fsm_evt_rx_rlrq,						fsm_state_unassociated,		&disassociating_release_response_tx}, // 6.16
	{fsm_state_waiting_for_config,	fsm_evt_rx_rlre,						fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 6.17
	{fsm_state_waiting_for_config,	fsm_evt_rx_abrt,						fsm_state_unassociated,		NULL}, // 6.18
	{fsm_state_waiting_for_config,	fsm_evt_rx_roiv_confirmed_event_report,				fsm_state_checking_config,	&configuring_perform_configuration}, // 6.24
	{fsm_state_waiting_for_config,	fsm_evt_rx_roiv_event_report,					fsm_state_waiting_for_config,	&communication_roer_tx}, // 6.25
	{fsm_state_waiting_for_config,	fsm_evt_rx_roiv_get,						fsm_state_waiting_for_config,	NULL}, // 6.25
	{fsm_state_waiting_for_config,	fsm_evt_rx_roiv_set,						fsm_state_waiting_for_config,	NULL}, // 6.25
	{fsm_state_waiting_for_config,	fsm_evt_rx_roiv_confirmed_set,					fsm_state_waiting_for_config,	NULL}, // 6.25
	{fsm_state_waiting_for_config,	fsm_evt_rx_roiv_action,						fsm_state_waiting_for_config,	NULL}, // 6.25
	{fsm_state_waiting_for_config,	fsm_evt_rx_roiv_confirmed_action,				fsm_state_waiting_for_config,	NULL}, // 6.25
	{fsm_state_waiting_for_config,	fsm_evt_rx_rors,						fsm_state_waiting_for_config,	&communication_check_invoke_id_abort_tx}, // 6.26
	// AB: Two following lines modified. Before they return NO RESPONSE as response instead of checking the invoke id
	{fsm_state_waiting_for_config,	fsm_evt_rx_roer,						fsm_state_waiting_for_config,	&communication_check_invoke_id_abort_tx}, // 6.26 - remark on page 147
	{fsm_state_waiting_for_config,	fsm_evt_rx_rorj,						fsm_state_waiting_for_config,	&communication_check_invoke_id_abort_tx}, // 6.26 - remark on page 147
	{fsm_state_waiting_for_config,	fsm_evt_req_agent_supplied_unknown_configuration,		fsm_state_waiting_for_config,	NULL}, // transcoding
	{fsm_state_waiting_for_config,	fsm_evt_req_agent_supplied_known_configuration,			fsm_state_operating,		NULL}, // transcoding


	{fsm_state_checking_config,	fsm_evt_ind_transport_disconnect,				fsm_state_disconnected,		&communication_disconnect_tx}, // 7.2
	{fsm_state_checking_config,	fsm_evt_ind_timeout,						fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 7.4
	{fsm_state_checking_config,	fsm_evt_req_assoc_rel,						fsm_state_disassociating,	&disassociating_release_request_tx}, // 7.6
	{fsm_state_checking_config,	fsm_evt_req_assoc_abort,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 7.7
	{fsm_state_checking_config,	fsm_evt_rx_aarq,		fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 7.8 - Added by AB. It was non present in the original fsm.c
	{fsm_state_checking_config,	fsm_evt_rx_aarq_acceptable_and_known_configuration,		fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 7.8
	{fsm_state_checking_config,	fsm_evt_rx_aarq_acceptable_and_unknown_configuration,		fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 7.8
	{fsm_state_checking_config,	fsm_evt_rx_aarq_unacceptable_configuration,			fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 7.8
	{fsm_state_checking_config,	fsm_evt_rx_aare,						fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 7.12
	{fsm_state_checking_config,	fsm_evt_rx_rlrq,						fsm_state_unassociated,		&disassociating_release_response_tx}, // 7.16
	{fsm_state_checking_config,	fsm_evt_rx_rlre,						fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 7.17 - remark on page 148
	{fsm_state_checking_config,	fsm_evt_rx_abrt,						fsm_state_unassociated,		NULL}, // 7.18
	{fsm_state_checking_config,	fsm_evt_rx_roiv_confirmed_event_report,				fsm_state_checking_config,	&configuring_new_measurements_response_tx}, // 7.24
	{fsm_state_checking_config,	fsm_evt_rx_roiv_all_except_confirmed_event_report,		fsm_state_unassociated,		&communication_roer_tx}, // 7.25
	{fsm_state_checking_config,	fsm_evt_rx_rors_confirmed_event_report,				fsm_state_checking_config,	NULL}, // 7.26
	{fsm_state_checking_config,	fsm_evt_rx_rors_get,						fsm_state_checking_config,	NULL}, // 7.26
	{fsm_state_checking_config,	fsm_evt_rx_rors_confirmed_set,					fsm_state_checking_config,	NULL}, // 7.26
	{fsm_state_checking_config,	fsm_evt_rx_rors_confirmed_action,				fsm_state_checking_config,	NULL}, // 7.26
	{fsm_state_checking_config,	fsm_evt_rx_roer,						fsm_state_checking_config,	NULL}, // 7.26
	{fsm_state_checking_config,	fsm_evt_rx_rorj,						fsm_state_checking_config,	NULL}, // 7.26
	{fsm_state_checking_config,	fsm_evt_req_agent_supplied_unknown_configuration,		fsm_state_waiting_for_config,	&configuring_configuration_response_tx}, // 7.31
	{fsm_state_checking_config,	fsm_evt_req_agent_supplied_known_configuration,			fsm_state_operating,		&configuring_configuration_response_tx}, // 7.32
	{fsm_state_checking_config,	fsm_evt_req_agent_supplied_bad_configuration,			fsm_state_waiting_for_config,	&configuring_configuration_rorj_tx}, // 7.32


	{fsm_state_operating,		fsm_evt_ind_transport_disconnect,				fsm_state_disconnected,		NULL}, // 8.2
	{fsm_state_operating,		fsm_evt_ind_timeout,						fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 8.4
	{fsm_state_operating,		fsm_evt_req_assoc_rel,						fsm_state_disassociating,	&operating_assoc_release_req_tx}, // 8.6
	{fsm_state_operating,		fsm_evt_req_assoc_abort,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 8.7
	{fsm_state_operating,		fsm_evt_rx_aarq,						fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 8.8
	{fsm_state_operating,		fsm_evt_rx_aare,						fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 8.12
	{fsm_state_operating,		fsm_evt_rx_rlrq,						fsm_state_unassociated,		&disassociating_release_response_tx}, // 8.16
	{fsm_state_operating,		fsm_evt_rx_rlre,						fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 8.17
	{fsm_state_operating,		fsm_evt_rx_abrt,						fsm_state_unassociated,		NULL}, // 8.18
	{fsm_state_operating,		fsm_evt_rx_roiv_confirmed_event_report,				fsm_state_operating,		&operating_event_report}, // 8.21
	{fsm_state_operating,		fsm_evt_rx_roiv_event_report,					fsm_state_operating,		&operating_event_report}, // 8.21
	{fsm_state_operating,		fsm_evt_rx_roiv_all_except_confirmed_event_report,		fsm_state_operating,		&operating_roiv_non_event_report}, // 8.21
	{fsm_state_operating,		fsm_evt_rx_rors_confirmed_event_report,				fsm_state_operating,		NULL}, // 8.26 - remark on page 149
	{fsm_state_operating,		fsm_evt_rx_rors_get,						fsm_state_operating,		&operating_get_response}, // 8.26 - remark on page 149
	{fsm_state_operating,		fsm_evt_rx_rors_confirmed_set,					fsm_state_operating,		&operating_set_scanner_response}, // 8.26 - remark on page 149
	{fsm_state_operating,		fsm_evt_rx_rors_confirmed_action,				fsm_state_operating,		&operating_rors_confirmed_action_tx}, // 8.26 - remark on page 149
	{fsm_state_operating,		fsm_evt_rx_roer,						fsm_state_operating,		&operating_roer_confirmed_action_tx}, // 8.26 - remark on page 149
	{fsm_state_operating,		fsm_evt_rx_rorj,						fsm_state_operating,		&operating_rorj_confirmed_action_tx}, // 8.26 - remark on page 149


	{fsm_state_disassociating,	fsm_evt_ind_transport_disconnect,				fsm_state_disconnected,		NULL}, // 9.2
	{fsm_state_disassociating,	fsm_evt_ind_timeout,						fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 9.4
	{fsm_state_disassociating,	fsm_evt_req_assoc_rel,						fsm_state_disassociating,	NULL}, // 9.6
	{fsm_state_disassociating,	fsm_evt_req_assoc_abort,					fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 9.7
	{fsm_state_disassociating,	fsm_evt_rx_aarq,						fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 9.8
	{fsm_state_disassociating,	fsm_evt_rx_aare,						fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 9.12
	{fsm_state_disassociating,	fsm_evt_rx_rlrq,						fsm_state_disassociating,	&disassociating_release_response_tx}, // 9.16
	{fsm_state_disassociating,	fsm_evt_rx_rlre,						fsm_state_unassociated,		&disassociating_release_proccess_completed}, // 9.17
	{fsm_state_disassociating,	fsm_evt_rx_abrt,						fsm_state_unassociated,		NULL}, // 9.18
	{fsm_state_disassociating,	fsm_evt_rx_roiv,						fsm_state_disassociating,	NULL}, // 9.21
	//{fsm_state_disassociating,	fsm_evt_rx_rors,						fsm_state_unassociated,		&communication_check_invoke_id_abort_tx}, // 9.26 - remark on page 150
	// AB: Fixed: the IEEE specification requires abrt as response when rors is received in disassociating state
	{fsm_state_disassociating,	fsm_evt_rx_rors,						fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 9.26 - remark on page 150
	{fsm_state_disassociating,	fsm_evt_rx_roer,						fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 9.26 - remark on page 150
	{fsm_state_disassociating,	fsm_evt_rx_rorj,						fsm_state_unassociated,		&communication_abort_undefined_reason_tx}, // 9.26 - remark on page 150

};

/**
 * Initialize fsm with the states and transition rules of
 * IEEE 11073-20601 for Manager
 *
 * @param fsm
 */
void fsm_set_manager_state_table(FSM *fsm)
{
	int transition_table_size = sizeof(IEEE11073_20601_manager_state_table);
	int trasition_rule_size = sizeof(FsmTransitionRule);
	int table_size = transition_table_size / trasition_rule_size;
	fsm_init(fsm, fsm_state_disconnected, IEEE11073_20601_manager_state_table,
								table_size);
}

/** @} */
