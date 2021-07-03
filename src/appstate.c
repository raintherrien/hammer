#include "hammer/appstate.h"
#include "hammer/error.h"
#include <stdio.h>
#include <string.h>

#define MAX_TRANSITIONS 128

static struct {
	dltask  runner;
	dltask *appstate_task;
	struct appstate_transition *current_state_transition;

	/* Transition table */
	size_t transitions_count;
	struct appstate_transition transitions[MAX_TRANSITIONS];
} mgr;

static void appstate_manager_loop_async(DL_TASK_ARGS);

dltask *
appstate_register(size_t count,
                  struct appstate_transition transitions[count])
{
	mgr.runner = DL_TASK_INIT(appstate_manager_loop_async);

	/* Copy the transition table */
	if (count > MAX_TRANSITIONS)
		xpanicva("appstate_manager_create passed %zu transitions which is larger than the maximum table size of %zu", count, MAX_TRANSITIONS);
	mgr.transitions_count = count;
	memcpy(mgr.transitions, transitions, count * sizeof(*mgr.transitions));

	/* Invoke the initial transition */
	size_t init_state_count = 0;
	for (size_t i = 0; i < count; ++ i) {
		if (mgr.transitions[i].state == -1) {
			++ init_state_count;
			mgr.current_state_transition = &mgr.transitions[i];
			mgr.appstate_task = mgr.current_state_transition->enter_fn();
		}
	}
	if (init_state_count < 1) {
		errno = EINVAL;
		xpanic("appstate_register called with no initial transition (state == -1)");
	}
	if (init_state_count > 1) {
		errno = EINVAL;
		xpanic("appstate_register called with multiple initial transitions (state == -1)");
	}

	return &mgr.runner;
}

void
appstate_transition(int state)
{
	mgr.current_state_transition->exit_fn(mgr.appstate_task);
	for (size_t i = 0; i < mgr.transitions_count; ++ i) {
		if (mgr.transitions[i].state == state) {
			mgr.current_state_transition = &mgr.transitions[i];
			if (mgr.current_state_transition->enter_fn == NULL)
				dlterminate();
			else
				mgr.appstate_task = mgr.current_state_transition->enter_fn();
			return;
		}
	}
	errno = EINVAL;
	xpanicva("appstate_transition called with state outside transition table: %d", state);
}

static void
appstate_manager_loop_async(DL_TASK_ARGS)
{
	DL_TASK_ENTRY_VOID;

	/* Kick off the current appstate task and join it back to this loop */
	dlwait(&mgr.runner, 1);
	dlnext(mgr.appstate_task, &mgr.runner);
	dlasync(mgr.appstate_task);
}
