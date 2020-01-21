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
#include "src/communication/agent/agent_association.h"
#include "src/communication/manager/manager_association.h"
#include "src/communication/communication.h"
#include "src/communication/agent/agent_disassociating.h"
#include "src/communication/manager/manager_disassociating.h"
#include "src/communication/manager/manager_configuring.h"
#include "src/communication/agent/agent_configuring.h"
#include "src/communication/manager/manager_operating.h"
#include "src/communication/agent/agent_operating.h"
#include "src/communication/agent/agent_ops.h"
#include "src/util/log.h"

static char *fsm_state_strings[] = {
	"disconnected",
	"disassociating",
	"unassociated",
	"associating",
	"config_sending",
	"waiting_approval",
	"operating",
	"checking_config",
	"waiting_for_config"
};


static char *fsm_event_strings[] = {
	"fsm_evt_ind_transport_connection",
	"fsm_evt_ind_transport_disconnect",
	"fsm_evt_ind_timeout",
	"fsm_evt_req_assoc_rel",
	"fsm_evt_req_assoc_abort",
	"fsm_evt_req_agent_supplied_unknown_configuration",
	"fsm_evt_req_agent_supplied_known_configuration",
	"fsm_evt_req_agent_supplied_bad_configuration",
	"fsm_evt_req_send_config",
	"fsm_evt_req_send_event",
	"fsm_evt_req_assoc",
	"fsm_evt_rx_aarq",
	"fsm_evt_rx_aarq_acceptable_and_known_configuration",
	"fsm_evt_rx_aarq_acceptable_and_unknown_configuration",
	"fsm_evt_rx_aarq_unacceptable_configuration",
	"fsm_evt_rx_aare",
	"fsm_evt_rx_aare_rejected",
	"fsm_evt_rx_aare_accepted_known",
	"fsm_evt_rx_aare_accepted_unknown",
	"fsm_evt_rx_rlrq",
	"fsm_evt_rx_rlre",
	"fsm_evt_rx_abrt",
	"fsm_evt_rx_prst",
	"fsm_evt_rx_roiv",
	"fsm_evt_rx_roiv_event_report",
	"fsm_evt_rx_roiv_confirmed_event_report",
	"fsm_evt_rx_roiv_all_except_confirmed_event_report",
	"fsm_evt_rx_roiv_get",
	"fsm_evt_rx_roiv_set",
	"fsm_evt_rx_roiv_confirmed_set",
	"fsm_evt_rx_roiv_action",
	"fsm_evt_rx_roiv_confirmed_action",
	"fsm_evt_rx_rors",
	"fsm_evt_rx_rors_confirmed_event_report",
	"fsm_evt_rx_rors_confirmed_event_report_unknown",
	"fsm_evt_rx_rors_confirmed_event_report_known",
	"fsm_evt_rx_rors_get",
	"fsm_evt_rx_rors_confirmed_set",
	"fsm_evt_rx_rors_confirmed_action",
	"fsm_evt_rx_roer",
	"fsm_evt_rx_rorj"
};

/**
 * Construct the state machine
 * @return finite state machine
 */
FSM *fsm_instance()
{
	FSM *fsm = malloc(sizeof(struct FSM));
	return fsm;
}

/**
 * Destroy state machine Deallocate the memory pointed by *fsm
 *
 * @param fsm
 */
void fsm_destroy(FSM *fsm)
{
	free(fsm);
}

/**
 * Initialize the state machine before process the inputs
 *
 * @param fsm state machine
 * @param entry_point_state the initial state of FSM
 * @param transition_table the transition rules table
 * @param table_size size of transition table array
 */
void fsm_init(FSM *fsm, fsm_states entry_point_state, FsmTransitionRule *transition_table, int table_size)
{
	// Define entry point state
	fsm->state = entry_point_state;
	/* Initialize Transition Rules */
	fsm->transition_table = transition_table;
	fsm->transition_table_size = table_size;
}

/**
 * Process the input events applying the transition rules, e.g.:
 * transport indications (IND), APDU arrival/departure (Rx/Tx),
 * manager requests (REQ).
 *
 * @param ctx connection context
 * @param evt event
 * @param data event data
 *
 * @return FSM_PROCESS_EVT_RESULT_STATE_CHANGED if the state has been changed,
 *  FSM_PROCESS_EVT_RESULT_STATE_UNCHANGED otherwise
 */
FSM_PROCESS_EVT_STATUS fsm_process_evt(FSMContext *ctx, fsm_events evt, FSMEventData *data)
{
	FSM *fsm = ctx->fsm;

	DEBUG(" state machine(<%s>): process event <%s> ", fsm_state_to_string(fsm->state), fsm_event_to_string(evt));

	int i;
	FsmTransitionRule *rule = NULL;

	for (i = 0; i < fsm->transition_table_size; i++) {
		rule = &fsm->transition_table[i];

		if (fsm->state == rule->currentState && evt == rule->inputEvent) {
			int state_changed = fsm->state != rule->nextState;

			// pre-action


			// Make transition
			DEBUG(" state machine(<%s>): transition to <%s> ",
				fsm_state_to_string(fsm->state), fsm_state_to_string(rule->nextState));


			fsm->state = rule->nextState;

			if (rule->post_action != NULL) {
				// pos-action
				(rule->post_action)(ctx, evt, data);
			}

			if (state_changed) {
				return FSM_PROCESS_EVT_RESULT_STATE_CHANGED;
			} else {
				return FSM_PROCESS_EVT_RESULT_STATE_UNCHANGED;
			}

			break;
		}
	}

	fflush(stdout);

	return FSM_PROCESS_EVT_RESULT_NOT_PROCESSED;
}

/**
 * Gets current state name
 * @param fsm State machine
 * @return the name of the state
 */
char *fsm_get_current_state_name(FSM *fsm)
{
	return fsm_state_to_string(fsm->state);
}

/**
 * Convert state code to text representation
 * @param state State machine's state value
 * @return string representation of sate
 */
char *fsm_state_to_string(fsm_states state)
{
	if ((int) state >= fsm_state_disconnected && state < fsm_state_size) {
		return fsm_state_strings[state];
	}

	return "not recognized";
}

/**
 * Convert event code to text representation
 * @param evt State machine event
 * @return string representation of event
 */
char *fsm_event_to_string(fsm_events evt)
{

	if ((int) evt >= fsm_evt_ind_transport_connection && evt < fsm_evt_size) {
		return fsm_event_strings[evt];
	}

	return "not recognized";
}

/** @} */
