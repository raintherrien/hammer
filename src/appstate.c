#include "hammer/appstate.h"
#include "hammer/appstate/main_menu.h"
#include "hammer/appstate/planet_generation.h"
#include "hammer/appstate/region_generation.h"
#include "hammer/appstate/world_config.h"
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
	case APPSTATE_TRANSITION_CLOSE_MAIN_MENU:
		appstate_main_menu_teardown();
		dlterminate();
		break;
	case APPSTATE_TRANSITION_CONFIGURE_NEW_WORLD:
		appstate_main_menu_teardown();
		appstate_world_config_setup();
		appstate_manager.appstate_task = &appstate_world_config_frame;
		break;
	case APPSTATE_TRANSITION_CONFIGURE_NEW_WORLD_CANCEL:
		appstate_world_config_teardown();
		appstate_main_menu_setup();
		appstate_manager.appstate_task = &appstate_main_menu_frame;
		break;
	case APPSTATE_TRANSITION_CREATE_NEW_PLANET:
		appstate_world_config_teardown();
		appstate_planet_generation_setup();
		appstate_manager.appstate_task = &appstate_planet_generation_frame;
		break;
	case APPSTATE_TRANSITION_CREATE_NEW_PLANET_CANCEL:
		appstate_planet_generation_teardown();
		appstate_main_menu_setup();
		appstate_manager.appstate_task = &appstate_main_menu_frame;
		break;
	case APPSTATE_TRANSITION_CHOOSE_REGION:
		/* Leave planet constructed */
		appstate_region_generation_setup();
		appstate_manager.appstate_task = &appstate_region_generation_frame;
		break;
	case APPSTATE_TRANSITION_CHOOSE_REGION_CANCEL:
		/* Planet still constructed */
		appstate_planet_generation_reset_region_selection();
		appstate_region_generation_teardown_discard_region();
		appstate_manager.appstate_task = &appstate_planet_generation_frame;
		break;
	case APPSTATE_TRANSITION_CONFIRM_REGION_AND_ENTER_WORLD:
	case APPSTATE_TRANSITION_EXIT_WORLD:
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
