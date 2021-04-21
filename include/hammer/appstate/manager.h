#ifndef HAMMER_APPSTATE_MANAGER_H_
#define HAMMER_APPSTATE_MANAGER_H_

#include <deadlock/dl.h>

struct appstate_cmd;

typedef void(*appstate_entry_fn)(void *);
typedef void(*appstate_exit_fn)(void *);
typedef void(*appstate_loop_fn)(void *, struct appstate_cmd *);

struct appstate {
	appstate_entry_fn entry_fn;
	appstate_exit_fn exit_fn;
	appstate_loop_fn loop_fn;
	void *arg;
};

enum appstate_cmd_type {
	APPSTATE_CMD_LOOP,
	APPSTATE_CMD_POP,
	APPSTATE_CMD_PUSH,
	APPSTATE_CMD_SWAP
};

struct appstate_cmd {
	enum appstate_cmd_type type;
	struct appstate newstate;
};

struct appstate_manager {
	dltask task;
	struct appstate *states; /* vector */
};

void appstate_manager_create(struct appstate_manager *, struct appstate seed);
void appstate_manager_destroy(struct appstate_manager *);

#endif /* HAMMER_APPSTATE_MANAGER_H_ */
