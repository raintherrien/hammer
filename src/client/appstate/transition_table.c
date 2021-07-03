#include "hammer/client/appstate/main_menu.h"
#include "hammer/client/appstate/transition_table.h"

struct appstate_transition client_tt[] = {
	{
		APPSTATE_OLD_STATE_INITIAL,
		CLIENT_APPSTATE_TRANSITION_MAIN_MENU_OPEN,
		appstate_main_menu_enter,
		appstate_main_menu_exit
	},
	{
		CLIENT_APPSTATE_TRANSITION_MAIN_MENU_OPEN,
		CLIENT_APPSTATE_TRANSITION_MAIN_MENU_CLOSE
	}
};

size_t client_tt_sz = sizeof(client_tt) / sizeof(*client_tt);

struct appstate_manager client_appstate_mgr;
