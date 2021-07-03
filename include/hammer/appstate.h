#ifndef HAMMER_APPSTATE_H_
#define HAMMER_APPSTATE_H_

#include <deadlock/dl.h>

struct appstate_transition {
	int     state;
	dltask *(*enter_fn)(void);
	void    (* exit_fn)(dltask *);
};

/*
 * Returns a task which transfers control to the appstate system registered to
 * this process. This may be called at most once by a single process.
 */
dltask *appstate_register(size_t transitions_count,
                          struct appstate_transition transitions[*]);

/*
 * Immediately signals to the appstate system that it should perform a
 * transition from one state to another.
 *
 * This should only be called immediately before returning from a state task.
 */
void appstate_transition(int state);

#endif /* HAMMER_APPSTATE_H_ */
