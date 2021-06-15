#include "hammer/appstate.h"
#include "hammer/chunkmgr.h"
#include "hammer/glthread.h"
#include "hammer/server.h"
#include "hammer/window.h"
#include <stdio.h> // xxx

/* TODO: Rename game to client */

dltask appstate_game_frame;

struct chunkmesh { int xxx; };

static struct {
	struct chunkmgr chunkmgr;
	struct map3 chunkmesh_map;
	struct pool chunkmesh_pool;
} game;

static void game_frame_async(DL_TASK_ARGS);
static int game_gl_setup(void *);
static int game_gl_frame(void *);

void
appstate_game_setup(void)
{
	appstate_game_frame = DL_TASK_INIT(game_frame_async);
	chunkmgr_create(&game.chunkmgr, &world.region);
	map3_create(&game.chunkmesh_map);
	pool_create(&game.chunkmesh_pool, sizeof(struct chunkmesh));

	/* XXX Generate some chunks */
	const int chunks = (int)game.chunkmgr.region->rect_size / CHUNK_LEN;
	const int half = chunks / 2;
	const int y = 0;
	for (int r = 0; r < chunks; ++ r)
	for (int q = 0; q < chunks; ++ q) {
		int s = half - (r - half) - (q - half);
		if (s > chunks)
			continue;
		chunkmgr_create_at(&game.chunkmgr, y, r, q);
	}
	/* XXX Mesh those chunks */
	size_t chunk_count = game.chunkmgr.chunk_map.entries_size;
	for (size_t i = 0; i < chunk_count; ++ i) {
		struct map3_entry *e = &game.chunkmgr.chunk_map.entries[i];
		if (!map3_isvalid(e))
			continue;
		//struct chunkmesh *mesh = XXX mesh chunk;
		//map3_put(&game.chunkmesh_map, e->x, e->y, e->z, mesh);
	}

	glthread_execute(game_gl_setup, NULL);
}

void
appstate_game_teardown(void)
{
	chunkmgr_destroy(&game.chunkmgr);
	pool_destroy(&game.chunkmesh_pool);
	map3_destroy(&game.chunkmesh_map);
}

static void
game_frame_async(DL_TASK_ARGS)
{
	DL_TASK_ENTRY_VOID;

	if (glthread_execute(game_gl_frame, NULL)) {
		appstate_transition(APPSTATE_TRANSITION_EXIT_GAME);
		return;
	}
	puts("frame");
}

static int
game_gl_setup(void *_)
{
	glClearColor(117 / 255.0f, 183 / 255.0f, 224 / 255.0f, 1);
	return 0;
}

static int
game_gl_frame(void *_)
{
	//XXX Shader, MVP etc. uniforms

	/* Render chunks */
	size_t chunk_count = game.chunkmesh_map.entries_size;
	for (size_t i = 0; i < chunk_count; ++ i) {
		struct map3_entry *e = &game.chunkmesh_map.entries[i];
		if (!map3_isvalid(e))
			continue;
		//XXX draw mesh
	}

	return window.should_close;
}
