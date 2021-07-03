#ifndef HAMMER_CLIENT_APPSTATE_TRANSITION_TABLE_H_
#define HAMMER_CLIENT_APPSTATE_TRANSITION_TABLE_H_

#include "hammer/appstate.h"

enum {
	CLIENT_APPSTATE_TRANSITION_MAIN_MENU_CLOSE = APPSTATE_NEW_STATE_TERMINATE,

	CLIENT_APPSTATE_TRANSITION_MAIN_MENU_OPEN = 0,
	CLIENT_APPSTATE_TRANSITION_LAUNCH_LOCAL_SERVER
};

extern size_t client_tt_sz;
extern struct appstate_transition client_tt[];

extern struct appstate_manager client_appstate_mgr;

#endif /* HAMMER_CLIENT_APPSTATE_TRANSITIONS_H_ */
