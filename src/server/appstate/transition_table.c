#include "hammer/server/appstate/planet_generation.h"
#include "hammer/server/appstate/start_local.h"
#include "hammer/server/appstate/transition_table.h"

struct appstate_transition local_server_tt[] = {
	/* Initial transition for local server, network set up by client */
	{
		SERVER_APPSTATE_TRANSITION_START_LOCAL,
		appstate_start_local_enter,
		appstate_start_local_exit,
	},

	/* We've been sent our configuration, begin generation */
	{
		SERVER_APPSTATE_TRANSITION_PLANET_GENERATION,
		appstate_planet_generation_enter,
		appstate_planet_generation_exit
	}
};

size_t local_server_tt_sz = sizeof(local_server_tt) / sizeof(*local_server_tt);

struct appstate_manager server_appstate_mgr;
