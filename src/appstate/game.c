#include "hammer/appstate/game.h"
#include "hammer/glthread.h"
#include "hammer/mem.h"
#include "hammer/window.h"
#include <stdio.h>

struct game_appstate {
	dltask task;
};

static void game_entry(DL_TASK_ARGS);
static void game_exit (DL_TASK_ARGS);
static void game_loop (DL_TASK_ARGS);

static int game_gl_setup(void *);
static int game_gl_frame(void *);

dltask *
game_appstate_alloc_detached(void)
{
	struct game_appstate *game = xcalloc(1, sizeof(*game));
	game->task = DL_TASK_INIT(game_entry);
	return &game->task;
}

static void
game_entry(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct game_appstate, game, task);
	glthread_execute(game_gl_setup, game);
	dltail(&game->task, game_loop);
}


static void
game_exit(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct game_appstate, game, task);
	free(game);
}

static void
game_loop(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct game_appstate, game, task);
	if (glthread_execute(game_gl_frame, game)) {
		dltail(&game->task, game_exit);
		return;
	}
	puts("blorp");
	dltail(&game->task, game_exit);
}

static int
game_gl_setup(void *game_)
{
	(void) game_;
	glClearColor(49 / 255.0f, 59 / 255.0f, 58 / 255.0f, 1);
	return 0;
}

static int
game_gl_frame(void *game_)
{
	(void) game_;
	return window.should_close;
}
