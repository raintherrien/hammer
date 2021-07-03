#include "hammer/client/appstate.h"
#include "hammer/client/appstate/godmode.h"
#include "hammer/client/appstate/main_menu.h"
#include "hammer/client/appstate/server_config.h"
#include "hammer/client/appstate/server_planet_gen.h"
#include "hammer/client/appstate/server_region_gen.h"
#include "hammer/error.h"
#include <stdio.h>

static struct {
	dltask runner;
	dltask *appstate_task;
} appstate_manager;

static void appstate_manager_loop_async(DL_TASK_ARGS);

dltask *
appstate_runner(void)
{
	appstate_manager.runner = DL_TASK_INIT(appstate_manager_loop_async);

	/* Initial appstate is the main menu */
	appstate_main_menu_setup();
	appstate_manager.appstate_task = &appstate_main_menu_frame;

	return &appstate_manager.runner;
}

void
appstate_transition(int transition)
{
	switch (transition) {
	case APPSTATE_TRANSITION_MAIN_MENU_CLOSE:
		appstate_main_menu_teardown();
		dlterminate();
		break;
	case APPSTATE_TRANSITION_SERVER_CONFIG:
		appstate_main_menu_teardown();
		appstate_server_config_setup();
		appstate_manager.appstate_task = &appstate_server_config_frame;
		break;
	case APPSTATE_TRANSITION_SERVER_CONFIG_CANCEL:
		appstate_server_config_teardown();
		appstate_main_menu_setup();
		appstate_manager.appstate_task = &appstate_main_menu_frame;
		break;
	case APPSTATE_TRANSITION_SERVER_PLANET_GEN:
		appstate_server_config_teardown();
		appstate_server_planet_gen_setup();
		appstate_manager.appstate_task = &appstate_server_planet_gen_frame;
		break;
	case APPSTATE_TRANSITION_SERVER_PLANET_GEN_CANCEL:
		appstate_server_planet_gen_teardown();
		appstate_main_menu_setup();
		appstate_manager.appstate_task = &appstate_main_menu_frame;
		break;
	case APPSTATE_TRANSITION_SERVER_REGION_GEN:
		/* Leave planet constructed */
		appstate_server_region_gen_setup();
		appstate_manager.appstate_task = &appstate_server_region_gen_frame;
		break;
	case APPSTATE_TRANSITION_SERVER_REGION_GEN_CANCEL:
		/* Planet still constructed */
		appstate_server_planet_gen_reset_region_selection();
		appstate_server_region_gen_teardown_discard_region();
		appstate_manager.appstate_task = &appstate_server_planet_gen_frame;
		break;
	case APPSTATE_TRANSITION_CONFIRM_REGION:
		/* Planet still constructed */
		appstate_server_region_gen_teardown_confirm_region();
		appstate_server_planet_gen_teardown();
		appstate_godmode_setup();
		appstate_manager.appstate_task = &appstate_godmode_frame;
		break;
	case APPSTATE_TRANSITION_CLIENT_CLOSE:
		appstate_godmode_teardown();
		appstate_main_menu_setup();
		appstate_manager.appstate_task = &appstate_main_menu_frame;
		break;
	case APPSTATE_TRANSITION_OPEN_IN_GAME_OPTIONS:
	case APPSTATE_TRANSITION_CLOSE_IN_GAME_OPTIONS:
	default:
		errno = EINVAL;
		xperrorva("Invalid appstate transition: %d", transition);
	}
}

static void
appstate_manager_loop_async(DL_TASK_ARGS)
{
	DL_TASK_ENTRY_VOID;

	/* Kick off the current appstate task and join it back to this loop */
	dlwait(&appstate_manager.runner, 1);
	dlnext(appstate_manager.appstate_task, &appstate_manager.runner);
	dlasync(appstate_manager.appstate_task);
}
