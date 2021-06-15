#include "hammer/appstate.h"
#include "hammer/chunkmgr.h"
#include "hammer/glthread.h"
#include "hammer/server.h"
#include "hammer/window.h"
#include <stdio.h> // xxx

// xxx shouldn't reference server

dltask appstate_client_frame;

struct chunkmesh { int xxx; };

static struct {
	struct chunkmgr chunkmgr;
	struct map3 chunkmesh_map;
	struct pool chunkmesh_pool;
} client;

static void client_frame_async(DL_TASK_ARGS);
static int client_gl_setup(void *);
static int client_gl_frame(void *);

void
appstate_client_setup(void)
{
	appstate_client_frame = DL_TASK_INIT(client_frame_async);
	chunkmgr_create(&client.chunkmgr, &server.world.region);
	map3_create(&client.chunkmesh_map);
	pool_create(&client.chunkmesh_pool, sizeof(struct chunkmesh));

	/* XXX Generate some chunks */
	const int chunks = (int)client.chunkmgr.region->rect_size / CHUNK_LEN;
	const int half = chunks / 2;
	const int y = 0;
	for (int r = 0; r < chunks; ++ r)
	for (int q = 0; q < chunks; ++ q) {
		int s = half - (r - half) - (q - half);
		if (s > chunks)
			continue;
		chunkmgr_create_at(&client.chunkmgr, y, r, q);
	}
	/* XXX Mesh those chunks */
	size_t chunk_count = client.chunkmgr.chunk_map.entries_size;
	for (size_t i = 0; i < chunk_count; ++ i) {
		struct map3_entry *e = &client.chunkmgr.chunk_map.entries[i];
		if (!map3_isvalid(e))
			continue;
		//struct chunkmesh *mesh = XXX mesh chunk;
		//map3_put(&client.chunkmesh_map, e->x, e->y, e->z, mesh);
	}

	glthread_execute(client_gl_setup, NULL);
}

void
appstate_client_teardown(void)
{
	chunkmgr_destroy(&client.chunkmgr);
	pool_destroy(&client.chunkmesh_pool);
	map3_destroy(&client.chunkmesh_map);
}

static void
client_frame_async(DL_TASK_ARGS)
{
	DL_TASK_ENTRY_VOID;

	if (glthread_execute(client_gl_frame, NULL)) {
		appstate_transition(APPSTATE_TRANSITION_CLIENT_CLOSE);
		return;
	}
	puts("frame");
}

static int
client_gl_setup(void *_)
{
	glClearColor(117 / 255.0f, 183 / 255.0f, 224 / 255.0f, 1);
	return 0;
}

static int
client_gl_frame(void *_)
{
	//XXX Shader, MVP etc. uniforms

	/* Render chunks */
	size_t chunk_count = client.chunkmesh_map.entries_size;
	for (size_t i = 0; i < chunk_count; ++ i) {
		struct map3_entry *e = &client.chunkmesh_map.entries[i];
		if (!map3_isvalid(e))
			continue;
		//XXX draw mesh
	}

	return window.should_close;
}
