#ifndef HAMMER_APPSTATE_MAIN_MENU_H_
#define HAMMER_APPSTATE_MAIN_MENU_H_

#include "hammer/cli.h"
#include "hammer/gui.h"

#define VERSION_STR_MAX_LEN 128

struct appstate_main_menu {
	struct rtargs args;
	gui_btn_state exit_btn_state;
	gui_btn_state generate_new_world_btn_state;
	char version_str[VERSION_STR_MAX_LEN];
};

struct appstate appstate_main_menu_alloc(struct rtargs);

#endif /* HAMMER_APPSTATE_MAIN_MENU_H_ */
