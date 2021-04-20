#ifndef HAMMER_GAME_STATES_WORLDGEN_CONFIG_MENU_H_
#define HAMMER_GAME_STATES_WORLDGEN_CONFIG_MENU_H_

#include "hammer/cli.h"
#include "hammer/gui.h"
#include <deadlock/dl.h>

#define WORLDGEN_SIZE_SEED_EDIT_BUFFER_LEN 32

struct worldgen_config_menu_pkg {
	dltask        task;
	struct rtargs args;

	/* private */
	enum {
		WORLDGEN_LOOP,
		WORLDGEN_EXIT,
		WORLDGEN_NEXT
	} frame_result;
	gui_button_state next_button_state;
	gui_button_state exit_button_state;
	char size_edit_buffer[WORLDGEN_SIZE_SEED_EDIT_BUFFER_LEN];
	char seed_edit_buffer[WORLDGEN_SIZE_SEED_EDIT_BUFFER_LEN];
};
struct worldgen_config_menu_pkg worldgen_config_menu_init(struct rtargs);

#endif /* HAMMER_GAME_STATES_WORLDGEN_CONFIG_MENU_H_ */
