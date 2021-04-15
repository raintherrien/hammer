#ifndef HAMMER_GAME_STATES_MAIN_MENU_H_
#define HAMMER_GAME_STATES_MAIN_MENU_H_

#include "hammer/cli.h"
#include <deadlock/dl.h>

#define VERSION_STR_MAX_LEN 128

struct main_menu_pkg {
	dltask        task;
	struct rtargs args;
	size_t        version_str_len;
	char          version_str[VERSION_STR_MAX_LEN];
};
void main_menu_entry(DL_TASK_ARGS);

#endif /* HAMMER_GAME_STATES_MAIN_MENU_H_ */
