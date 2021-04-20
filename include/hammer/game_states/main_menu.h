#ifndef HAMMER_GAME_STATES_MAIN_MENU_H_
#define HAMMER_GAME_STATES_MAIN_MENU_H_

#include "hammer/cli.h"
#include "hammer/gui.h"
#include <deadlock/dl.h>

#define VERSION_STR_MAX_LEN 128

struct main_menu_pkg {
	dltask        task;
	struct rtargs args;

	/* private */
	enum {
		MAIN_MENU_LOOP,
		MAIN_MENU_EXIT,
		MAIN_MENU_GENERATE_NEW_WORLD
	} frame_result;
	gui_button_state exit_button_state;
	gui_button_state generate_new_world_button_state;
	char version_str[VERSION_STR_MAX_LEN];
};

struct main_menu_pkg main_menu_pkg_init(struct rtargs);

#endif /* HAMMER_GAME_STATES_MAIN_MENU_H_ */
