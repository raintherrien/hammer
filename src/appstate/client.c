#include "hammer/appstate.h"
#include "hammer/chunkmgr.h"
#include "hammer/client/chunkmesh.h"
#include "hammer/glthread.h"
#include "hammer/math.h"
#include "hammer/server.h"
#include "hammer/window.h"
#include <cglm/cam.h>
#include <cglm/euler.h>
#include <stdio.h> // xxx

#define MIN_PITCH (-M_PI/2+0.001f)
#define MAX_PITCH ( M_PI/2-0.001f)

// xxx shouldn't reference server

dltask appstate_client_frame;

static struct {
	struct chunkmgr chunkmgr;
	struct map3 chunkmesh_map;
	struct pool chunkmesh_pool;
	struct {
		vec3 position;
		vec3 rotation;
		vec3 forward;
		vec3 right;
	} camera;
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

	glm_vec3_copy((vec3) { 0, 100, 0 }, client.camera.position);
	glm_vec3_copy((vec3) { MIN_PITCH, 0, 0 }, client.camera.rotation);
	glm_vec3_zero(client.camera.forward);
	glm_vec3_zero(client.camera.right);

	/* XXX Generate some chunks */
	const int chunks = 8;//(int)client.chunkmgr.region->rect_size / CHUNK_LEN;
	const int y = 0;
	for (int r = 0; r < chunks; ++ r)
	for (int q = 0; q < chunks; ++ q) {
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
		chunkmesh_gl_create(mesh, e->data, e->key[0], e->key[1], e->key[2]);
		map3_put(&client.chunkmesh_map, e->key, mesh);
	}

	glClearColor(117 / 255.0f, 183 / 255.0f, 224 / 255.0f, 1);
	window_lock_mouse(); /* XXX */
	return 0;
}

static int
client_gl_frame(void *_)
{
	client.camera.rotation[1] -= window.motion_x / 600.0f;
	client.camera.rotation[0] -= window.motion_y / 600.0f;
	client.camera.rotation[0] = CLAMP(client.camera.rotation[0], MIN_PITCH, MAX_PITCH);

	/* XXX Belongs elsewhere */
	vec3 opengl_up3 = { 0, 1, 0 };
	glm_vec3_copy((vec3) { 0, 0, -1 }, client.camera.forward);
	glm_vec3_rotate(client.camera.forward, client.camera.rotation[0], (vec3) { 1, 0, 0 });
	glm_vec3_rotate(client.camera.forward, client.camera.rotation[1], (vec3) { 0, 1, 0 });
	glm_vec3_cross(client.camera.forward, opengl_up3, client.camera.right);
	glm_vec3_normalize(client.camera.right);

	if (window.keydown[SDL_SCANCODE_W]) {
		glm_vec3_add(client.camera.position, client.camera.forward, client.camera.position);
	}

	if (window.keydown[SDL_SCANCODE_S]) {
		glm_vec3_sub(client.camera.position, client.camera.forward, client.camera.position);
	}

	if (window.keydown[SDL_SCANCODE_A]) {
		glm_vec3_sub(client.camera.position, client.camera.right, client.camera.position);
	}

	if (window.keydown[SDL_SCANCODE_D]) {
		glm_vec3_add(client.camera.position, client.camera.right, client.camera.position);
	}

	//XXX Shader, MVP etc. uniforms

	/* Arcball camera rotates around region */
	const float fov = glm_rad(60);
	mat4 view, proj, mvp;
	float aspect = window.width / (float)window.height;
	glm_perspective(fov, aspect, 1, 1000, proj);
	glm_look(client.camera.position, client.camera.forward, opengl_up3, view);
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
