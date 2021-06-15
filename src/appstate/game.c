#include "hammer/appstate/game.h"
#include "hammer/chunkmgr.h"
#include "hammer/glthread.h"
#include "hammer/map3.h"
#include "hammer/mem.h"
#include "hammer/pool.h"
#include "hammer/window.h"
#include "hammer/world/chunk.h"
#include "hammer/worldgen/region.h"
#include <stdio.h>

/* XXX Shaders etc. never freed */

struct chunkmesh { int bork; };

struct game_appstate {
	dltask task;
	struct chunkmgr chunkmgr;
	struct map3 chunkmesh_map;
	struct pool chunkmesh_pool;
};

static void game_entry(DL_TASK_ARGS);
static void game_exit (DL_TASK_ARGS);
static void game_loop (DL_TASK_ARGS);

static int game_gl_setup(void *);
static int game_gl_frame(void *);

dltask *
game_appstate_alloc_detached(const struct region *region)
{
	struct game_appstate *game = xcalloc(1, sizeof(*game));
	game->task = DL_TASK_INIT(game_entry);
	chunkmgr_create(&game->chunkmgr, region);
	map3_create(&game->chunkmesh_map);
	pool_create(&game->chunkmesh_pool, sizeof(struct chunkmesh));
	return &game->task;
}

static void
game_entry(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct game_appstate, game, task);
	glthread_execute(game_gl_setup, game);
	/* XXX Generate some chunks */
	const int chunks = (int)game->chunkmgr.region->rect_size / CHUNK_LEN;
	const int half = chunks / 2;
	const int y = 0;
	for (int r = 0; r < chunks; ++ r)
	for (int q = 0; q < chunks; ++ q) {
		int s = half - (r - half) - (q - half);
		if (s > chunks)
			continue;
		chunkmgr_create_at(&game->chunkmgr, y, r, q);
	}
	/* XXX Mesh those chunks */
	size_t chunk_count = game->chunkmgr.chunk_map.entries_size;
	for (size_t i = 0; i < chunk_count; ++ i) {
		struct map3_entry *e = &game->chunkmgr.chunk_map.entries[i];
		if (!map3_isvalid(e))
			continue;
		//struct chunkmesh *mesh = XXX mesh chunk;
		//map3_put(&game->chunkmesh_map, e->x, e->y, e->z, mesh);
	}
	dltail(&game->task, game_loop);
}


static void
game_exit(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct game_appstate, game, task);
	chunkmgr_destroy(&game->chunkmgr);
	pool_destroy(&game->chunkmesh_pool);
	map3_destroy(&game->chunkmesh_map);
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
	dltail(&game->task, game_loop);
}

static int
game_gl_setup(void *game_)
{
	struct game_appstate *game = game_;
	glClearColor(117 / 255.0f, 183 / 255.0f, 224 / 255.0f, 1);
	//XXX create shader
	return 0;
}

static int
game_gl_frame(void *game_)
{
	struct game_appstate *game = game_;

	//XXX Shader, MVP etc. uniforms

	/* Render chunks */
	size_t chunk_count = game->chunkmesh_map.entries_size;
	for (size_t i = 0; i < chunk_count; ++ i) {
		struct map3_entry *e = &game->chunkmesh_map.entries[i];
		if (!map3_isvalid(e))
			continue;
		//XXX draw mesh
	}

	return window.should_close;
}
