#ifndef HAMMER_APPSTATE_H_
#define HAMMER_APPSTATE_H_

#include <deadlock/dl.h>
#include <limits.h>

#define APPSTATE_MAX_TRANSITIONS 64

/*
 * An appstate transition defines operations to be performed when
 * transitioning from the current state to a new state.
 *
 * enter_fn of the transition occurring is invoked prior to exit_fn of the
 * prior transition and both are passed pointers to the prior appstate dltask
 * to facilitate handing off resources.
 */
struct appstate_transition {
	int     state;
	dltask *(*enter_fn)(void *arg);
	void    (* exit_fn)(dltask *old_state_appstate);
};

/*
 * A transition table must have exactly one transition with a state of
 * APPSTATE_STATE_INITIAL. This transition is the initial transition performed
 * when the manager is registered.
 */
#define APPSTATE_STATE_INITIAL -1

/*
 * A transition table may have one or more transitions with a new_state of
 * APPSTATE_STATE_TERMINATE. When this transition is performed dlterminate()
 * is invoked after the prior appstate has exited.
 *
 * This is useful for terminating an application.
 */
#define APPSTATE_STATE_TERMINATE INT_MAX

/*
 * The appstate manager is your typical FSM implemented using a dltask
 * trampoline function. transition() is called from the currently executing
 * appstate to perform a transition.
 */
struct appstate_manager {
	/*
	 * The trampoline task is responsible for invoking the current
	 * appstate until notified of a transition, then performing the
	 * transition to the new appstate.
	 */
	dltask trampoline_;

	/*
	 * The currently executing appstate and the transition to this
	 * appstate.
	 */
	dltask *curr_appstate_;
	struct appstate_transition curr_transition_;

	/* Transition table */
	size_t transition_count_;
	struct appstate_transition transitions_[APPSTATE_MAX_TRANSITIONS];
};

/*
 * Sets up and returns a task which transfers control to the appstate manager.
 */
dltask *appstate_manager_init(struct appstate_manager *,
                              size_t transition_count,
                              struct appstate_transition transitions[*]);

/*
 * Immediately signals to the appstate system that it should perform a
 * transition from one state to another.
 *
 * This should only be called immediately before returning from a state task.
 */
void transition(struct appstate_manager *, int new_state);

#endif /* HAMMER_APPSTATE_H_ */
