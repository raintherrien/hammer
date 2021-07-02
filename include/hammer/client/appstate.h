#ifndef HAMMER_APPSTATE_H_
#define HAMMER_APPSTATE_H_

#include <deadlock/dl.h>

enum {
	APPSTATE_TRANSITION_MAIN_MENU_CLOSE,
	APPSTATE_TRANSITION_SERVER_CONFIG,
	APPSTATE_TRANSITION_SERVER_CONFIG_CANCEL,
	APPSTATE_TRANSITION_SERVER_PLANET_GEN,
	APPSTATE_TRANSITION_SERVER_PLANET_GEN_CANCEL,
	APPSTATE_TRANSITION_SERVER_REGION_GEN,
	APPSTATE_TRANSITION_SERVER_REGION_GEN_CANCEL,
	APPSTATE_TRANSITION_CONFIRM_REGION,
	APPSTATE_TRANSITION_CLIENT_CLOSE,
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
