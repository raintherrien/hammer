#ifndef HAMMER_GAME_STATES_WORLDGEN_MENU_H_
#define HAMMER_GAME_STATES_WORLDGEN_MENU_H_

#include "hammer/cli.h"
#include "hammer/gui.h"
#include <deadlock/dl.h>

struct worldgen_menu_pkg {
	dltask        task;
	struct rtargs args;

	/* private */
	enum {
		WORLDGEN_LOOP,
		WORLDGEN_EXIT
	} frame_result;
	gui_button_state exit_button_state;
};
struct worldgen_menu_pkg worldgen_menu_pkg_init(struct rtargs);

#endif /* HAMMER_GAME_STATES_WORLDGEN_MENU_H_ */
