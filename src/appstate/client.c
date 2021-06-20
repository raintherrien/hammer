#include "hammer/appstate.h"
#include "hammer/chunkmgr.h"
#include "hammer/client/chunkmesh.h"
#include "hammer/glthread.h"
#include "hammer/math.h"
#include "hammer/server.h"
#include "hammer/window.h"
#include <cglm/cam.h>
#include <stdio.h> // xxx

// xxx shouldn't reference server

dltask appstate_client_frame;

static struct {
	struct chunkmgr chunkmgr;
	struct map3 chunkmesh_map;
	struct pool chunkmesh_pool;
	float yaw;
	float pitch;
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

	client.yaw = -M_PI / 2;
	client.pitch = -FLT_EPSILON;

	/* XXX Generate some chunks */
	const int chunks = 2;//(int)client.chunkmgr.region->rect_size / CHUNK_LEN;
	const int half = chunks / 2;
	const int y = 0;
	for (int r = 0; r < chunks; ++ r)
	for (int q = 0; q < chunks; ++ q) {
		int s = half - (r - half) - (q - half);
		if (s > chunks)
			continue;
		chunkmgr_create_at(&client.chunkmgr, y, r, q);
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
	chunkmesh_renderer_gl_create();

	/* XXX Mesh those chunks */
	size_t chunk_count = client.chunkmgr.chunk_map.entries_size;
	for (size_t i = 0; i < chunk_count; ++ i) {
		struct map3_entry *e = &client.chunkmgr.chunk_map.entries[i];
		if (!map3_isvalid(e))
			continue;
		struct chunkmesh *mesh = pool_take(&client.chunkmesh_pool);
		chunkmesh_gl_create(mesh, e->data);
		map3_put(&client.chunkmesh_map, e->key, mesh);
	}

	glClearColor(117 / 255.0f, 183 / 255.0f, 224 / 255.0f, 1);
	return 0;
}

static int
client_gl_frame(void *_)
{
	client.yaw   += window.motion_x / 100.0f;
	client.pitch += window.motion_y / 100.0f;
	client.pitch = CLAMP(client.pitch, -M_PI/2, -FLT_EPSILON);
	//XXX Shader, MVP etc. uniforms

	/* Arcball camera rotates around region */
	const float r = 10;
	const float fov = glm_rad(60);
	mat4 view, proj, mvp;
	float aspect = window.width / (float)window.height;
	glm_perspective(fov, aspect, 1, 1000, proj);
	glm_lookat((vec3){r * sinf(client.pitch) * cosf(client.yaw),
	                  r * cosf(client.pitch),
	                  r * sinf(client.pitch) * sinf(client.yaw)},
	           (vec3){0, 0, 0},
	           (vec3){0, 1, 0},
	           view);
	glm_mat4_mulN((mat4 *[]){&proj, &view}, 2, mvp);

	glUseProgram(chunkmesh_renderer.shader);
	glUniformMatrix4fv(chunkmesh_renderer.uniforms.mvp, 1, GL_FALSE, (float *)mvp);

	/* Render chunks */
	size_t chunk_count = client.chunkmesh_map.entries_size;
	for (size_t i = 0; i < chunk_count; ++ i) {
		struct map3_entry *e = &client.chunkmesh_map.entries[i];
		if (!map3_isvalid(e))
			continue;
		struct chunkmesh *m = e->data;
		glBindVertexArray(m->vao);
		glDrawArrays(GL_TRIANGLES, 0, m->vc);
	}

	window_submitframe();
	return window.should_close;
}
