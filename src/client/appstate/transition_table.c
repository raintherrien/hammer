#include "hammer/client/appstate/main_menu.h"
#include "hammer/client/appstate/server_config.h"
#include "hammer/client/appstate/server_discover.h"
#include "hammer/client/appstate/transition_table.h"

struct appstate_transition client_tt[] = {
	/* Initial transition, open main menu */
	{
		CLIENT_APPSTATE_TRANSITION_MAIN_MENU_OPEN,
		appstate_main_menu_enter,
		appstate_main_menu_exit
	},

	/* Main menu launched a local server, discover it */
	{
		CLIENT_APPSTATE_TRANSITION_DISCOVER_SERVER,
		appstate_server_discover_enter,
		appstate_server_discover_exit
	},

	/* Server discovered in configurable state */
	{
		CLIENT_APPSTATE_TRANSITION_CONFIGURE_SERVER,
		appstate_server_config_enter,
		appstate_server_config_exit
	}
};

size_t client_tt_sz = sizeof(client_tt) / sizeof(*client_tt);

struct appstate_manager client_appstate_mgr;
