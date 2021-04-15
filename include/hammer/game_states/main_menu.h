#ifndef HAMMER_GAME_STATES_MAIN_MENU_H_
#define HAMMER_GAME_STATES_MAIN_MENU_H_

#include "hammer/cli.h"
#include <deadlock/dl.h>

struct main_menu_pkg {
	dltask        task;
	struct rtargs args;
};
void main_menu_entry(DL_TASK_ARGS);

#endif /* HAMMER_GAME_STATES_MAIN_MENU_H_ */
