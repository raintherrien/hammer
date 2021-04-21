#ifndef HAMMER_APPSTATE_WORLD_CONFIG_H_
#define HAMMER_APPSTATE_WORLD_CONFIG_H_

#include "hammer/cli.h"
#include "hammer/gui.h"

#define WORLDCONF_NUM_EDIT_BUFFER_LEN 32

struct appstate_world_config {
	struct rtargs args;
	gui_btn_state next_btn_state;
	gui_btn_state exit_btn_state;
	char size_edit_buf[WORLDCONF_NUM_EDIT_BUFFER_LEN];
	char seed_edit_buf[WORLDCONF_NUM_EDIT_BUFFER_LEN];
};

struct appstate appstate_world_config_alloc(struct rtargs);

#endif /* HAMMER_APPSTATE_WORLD_CONFIG_H_ */
