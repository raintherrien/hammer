#ifndef HAMMER_APPSTATE_H_
#define HAMMER_APPSTATE_H_

#include <deadlock/dl.h>

enum {
	APPSTATE_TRANSITION_CLOSE_MAIN_MENU,
	APPSTATE_TRANSITION_CONFIGURE_NEW_WORLD,
	APPSTATE_TRANSITION_CONFIGURE_NEW_WORLD_CANCEL,
	APPSTATE_TRANSITION_CREATE_NEW_PLANET,
	APPSTATE_TRANSITION_CREATE_NEW_PLANET_CANCEL,
	APPSTATE_TRANSITION_CHOOSE_REGION,
	APPSTATE_TRANSITION_CHOOSE_REGION_CANCEL,
	APPSTATE_TRANSITION_CONFIRM_REGION_AND_ENTER_GAME,
	APPSTATE_TRANSITION_EXIT_GAME,
	APPSTATE_TRANSITION_OPEN_IN_GAME_OPTIONS,
	APPSTATE_TRANSITION_CLOSE_IN_GAME_OPTIONS
};

/*
 * Returns a task which transfers control to the appstate system.
 */
dltask *appstate_runner(void);

/*
 * Immediately signals to the appstate system that it should perform a
 * transition from one state to another.
 *
 * This should only be called immediately before returning from a state task.
 */
void appstate_transition(int transition);

#endif /* HAMMER_APPSTATE_H_ */
