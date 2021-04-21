#include "hammer/appstate/manager.h"
#include "hammer/vector.h"
#include <stdlib.h>

static void appstate_manager_run(DL_TASK_ARGS);
static void appstate_manager_enter_tail(struct appstate_manager *);

void
appstate_manager_create(struct appstate_manager *mgr, struct appstate seed)
{
	*mgr = (struct appstate_manager) {
		.task = DL_TASK_INIT(appstate_manager_run),
		.states = NULL
	};
	vector_push(&mgr->states, seed);
	appstate_manager_enter_tail(mgr);
}

void
appstate_manager_destroy(struct appstate_manager *mgr)
{
	while (vector_size(mgr->states)) {
		struct appstate *s = vector_tail(mgr->states);
		s->exit_fn(s->arg);
		free(s->arg);
	}
	vector_free(&mgr->states);
}

static void
appstate_manager_run(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct appstate_manager, mgr, task);

	if (vector_size(mgr->states) == 0) {
		dlterminate();
		return;
	}

	struct appstate *current = vector_tail(mgr->states);
	struct appstate_cmd cmd = { .type = APPSTATE_CMD_LOOP };
	current->loop_fn(current->arg, &cmd);
	switch (cmd.type) {
	case APPSTATE_CMD_LOOP:
		/* no-op */
		break;
	case APPSTATE_CMD_POP:
		current->exit_fn(current->arg);
		free(current->arg);
		vector_pop(&mgr->states);
		if (vector_size(mgr->states)) {
			appstate_manager_enter_tail(mgr);
		}
		break;
	case APPSTATE_CMD_PUSH:
		current->exit_fn(current->arg);
		vector_push(&mgr->states, cmd.newstate);
		appstate_manager_enter_tail(mgr);
		break;
	case APPSTATE_CMD_SWAP:
		current->exit_fn(current->arg);
		free(current->arg);
		*current = cmd.newstate;
		appstate_manager_enter_tail(mgr);
		break;
	}

	dltail(&mgr->task);
}

static void
appstate_manager_enter_tail(struct appstate_manager *mgr)
{
	struct appstate *current = vector_tail(mgr->states);
	current->entry_fn(current->arg);
}
