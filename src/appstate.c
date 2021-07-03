#include "hammer/appstate.h"
#include "hammer/error.h"
#include <stdio.h>
#include <string.h>

static void appstate_manager_trampoline_async(DL_TASK_ARGS);

dltask *
appstate_manager_init(struct appstate_manager *mgr,
                      size_t transition_count,
                      struct appstate_transition transitions[transition_count])
{
	mgr->trampoline_ = DL_TASK_INIT(appstate_manager_trampoline_async);

	/* Copy the transition table */
	if (transition_count > APPSTATE_MAX_TRANSITIONS)
		xpanicva("appstate_manager_register passed %zu transitions which is larger than the maximum table size of %zu", transition_count, APPSTATE_MAX_TRANSITIONS);
	mgr->transition_count_ = transition_count;
	memcpy(mgr->transitions_, transitions, transition_count * sizeof(*mgr->transitions_));

	/* Invoke the initial transition */
	size_t init_state_count = 0;
	for (size_t i = 0; i < mgr->transition_count_; ++ i) {
		struct appstate_transition t = mgr->transitions_[i];
		if (t.state != APPSTATE_STATE_INITIAL)
			continue;

		++ init_state_count;
		mgr->curr_transition_ = t;
		mgr->curr_appstate_ = mgr->curr_transition_.enter_fn(NULL);
	}
	if (init_state_count < 1) {
		errno = EINVAL;
		xpanic("appstate_register called with no initial transition (state == -1)");
	}
	if (init_state_count > 1) {
		errno = EINVAL;
		xpanic("appstate_register called with multiple initial transitions (state == -1)");
	}

	return &mgr->trampoline_;
}

void
transition(struct appstate_manager *mgr, int new_state, void *new_state_arg)
{
	dltask *exiting_appstate = mgr->curr_appstate_;
	struct appstate_transition exiting = mgr->curr_transition_;

	if (new_state == APPSTATE_STATE_TERMINATE) {
		exiting.exit_fn(exiting_appstate);
		dlterminate();
		return;
	}

	for (size_t i = 0; i < mgr->transition_count_; ++ i) {
		struct appstate_transition t = mgr->transitions_[i];
		if (t.state == new_state) {
			mgr->curr_transition_ = t;
			mgr->curr_appstate_ = t.enter_fn(new_state_arg);
			goto exit_prior_appstate;
		}
	}

	errno = EINVAL;
	xpanicva("appstate_transition called with state outside transition table: %d", new_state);

exit_prior_appstate:
	exiting.exit_fn(exiting_appstate);
}

static void
appstate_manager_trampoline_async(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct appstate_manager, mgr, trampoline_);

	/* Kick off the current appstate task and join it back to this loop */
	dlwait(&mgr->trampoline_, 1);
	dlnext(mgr->curr_appstate_, &mgr->trampoline_);
	dlasync(mgr->curr_appstate_);
}
