#ifndef HAMMER_GAME_STATES_BASIC_WORLDGEN_H_
#define HAMMER_GAME_STATES_BASIC_WORLDGEN_H_

#include "hammer/cli.h"
#include "hammer/gui.h"
#include <deadlock/dl.h>

struct basic_worldgen_pkg {
	dltask        task;
	struct rtargs args;

	/* private */
	enum {
		BASIC_WORLDGEN_LOOP,
		BASIC_WORLDGEN_EXIT
	} frame_result;
	gui_button_state exit_button_state;
};
struct basic_worldgen_pkg basic_worldgen_init(struct rtargs);

#endif /* HAMMER_GAME_STATES_BASIC_WORLDGEN_H_ */
