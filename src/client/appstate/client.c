#include "hammer/appstate.h"
#include "hammer/chunkmgr.h"
#include "hammer/client/chunkmesh.h"
#include "hammer/glthread.h"
#include "hammer/hexagon.h"
#include "hammer/math.h"
#include "hammer/server.h"
#include "hammer/window.h"
#include <cglm/cam.h>
#include <cglm/euler.h>

#define MIN_PITCH (-M_PI/2+0.001f)
#define MAX_PITCH ( M_PI/2-0.001f)

/* TODO shouldn't reference server */

dltask appstate_client_frame;

static struct {
	struct chunkmgr chunkmgr;
	struct map3 chunkmesh_map;
	struct pool chunkmesh_pool;
	struct {
		vec3 position;
		vec3 rotation;
		vec3 forward;
		vec3 forward_horizontal;
		vec3 right;
		float fov;
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

	float half = server.world.region.size / 2;
	glm_vec3_copy((vec3) { half, 50, half }, client.camera.position);
	glm_vec3_copy((vec3) { MIN_PITCH, 0, 0 }, client.camera.rotation);
	glm_vec3_zero(client.camera.forward);
	glm_vec3_zero(client.camera.forward_horizontal);
	glm_vec3_zero(client.camera.right);
	client.camera.fov = 60;

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

	/* DEBUG: Generate chunks around camera for */
	float hex_pos[2];
	hex_pixel_to_axial(BLOCK_HEX_SIZE,
	                   client.camera.position[0],
	                   client.camera.position[2],
	                   hex_pos+0, hex_pos+1);
	fprintf(stderr, "(y,r,q)\t(%ld,%ld,%ld)\n",
	        (long)(client.camera.position[1] / CHUNK_LEN),
	        (long)(hex_pos[1] / CHUNK_LEN),
	        (long)(hex_pos[0] / CHUNK_LEN));
	float miny = client.camera.position[1] / CHUNK_LEN - 1;
	float maxy = client.camera.position[1] / CHUNK_LEN + 1;
	float maxr = hex_pos[1] / CHUNK_LEN + 10;
	float minr = hex_pos[1] / CHUNK_LEN - 10;
	float maxq = hex_pos[0] / CHUNK_LEN + 10;
	float minq = hex_pos[0] / CHUNK_LEN - 10;
	for (long y = miny; y <= maxy; ++ y)
	for (long r = minr; r <= maxr; ++ r)
	for (long q = minq; q <= maxq; ++ q) {
		if (chunkmgr_chunk_at(&client.chunkmgr, y, r, q) == NULL) {
			chunkmgr_create_at(&client.chunkmgr, y, r, q);
		}
	}
}

static int
client_gl_setup(void *_)
{
	chunkmesh_renderer_gl_create();
	glClearColor(117 / 255.0f, 183 / 255.0f, 224 / 255.0f, 1);
	window_lock_mouse(); /* DEBUG: Bad */
	return 0;
}

static int
client_gl_frame(void *_)
{
	client.camera.rotation[1] -= window.motion_x / 600.0f;
	client.camera.rotation[0] -= window.motion_y / 600.0f;
	client.camera.rotation[0] = CLAMP(client.camera.rotation[0], MIN_PITCH, MAX_PITCH);

	/* DEBUG: Mesh out debug chunks */
	size_t chunkmgr_entry_count = client.chunkmgr.chunk_map.entries_size;
	for (size_t i = 0; i < chunkmgr_entry_count; ++ i) {
		struct map3_entry *e = &client.chunkmgr.chunk_map.entries[i];
		if (!map3_isvalid(e) || map3_get(&client.chunkmesh_map, e->key) != NULL)
			continue;
		struct chunkmesh *mesh = pool_take(&client.chunkmesh_pool);
		chunkmesh_gl_create(mesh, e->data, e->key[0], e->key[1], e->key[2]);
		map3_put(&client.chunkmesh_map, e->key, mesh);
	}

	/* TODO: Belongs elsewhere */
	vec3 opengl_up = { 0, 1, 0 };
	glm_vec3_copy((vec3) { 0, 0, -1 }, client.camera.forward);
	glm_vec3_rotate(client.camera.forward, client.camera.rotation[0], (vec3) { 1, 0, 0 });
	glm_vec3_rotate(client.camera.forward, client.camera.rotation[1], (vec3) { 0, 1, 0 });
	glm_vec3_cross(client.camera.forward, opengl_up, client.camera.right);
	glm_vec3_normalize(client.camera.right);

	glm_vec3_copy(client.camera.forward, client.camera.forward_horizontal);
	client.camera.forward_horizontal[1] = 0;
	glm_vec3_normalize(client.camera.forward_horizontal);

	if (window.keydown[SDL_SCANCODE_W]) {
		glm_vec3_add(client.camera.position, client.camera.forward_horizontal, client.camera.position);
	}

	if (window.keydown[SDL_SCANCODE_S]) {
		glm_vec3_sub(client.camera.position, client.camera.forward_horizontal, client.camera.position);
	}

	if (window.keydown[SDL_SCANCODE_A]) {
		glm_vec3_sub(client.camera.position, client.camera.right, client.camera.position);
	}

	if (window.keydown[SDL_SCANCODE_D]) {
		glm_vec3_add(client.camera.position, client.camera.right, client.camera.position);
	}

	if (window.keydown[SDL_SCANCODE_SPACE]) {
		glm_vec3_add(client.camera.position, opengl_up, client.camera.position);
	}

	if (window.keydown[SDL_SCANCODE_LSHIFT]) {
		glm_vec3_sub(client.camera.position, opengl_up, client.camera.position);
	}

	if (window.keydown[SDL_SCANCODE_P]) {
		fprintf(stderr, "%f\t%f\t%f\n", client.camera.position[0],
		                                client.camera.position[1],
		                                client.camera.position[2]);
	}

	if (window.scroll)
	{
		client.camera.fov -= window.scroll / 10.0f;
		client.camera.fov = CLAMP(client.camera.fov, 30, 60);
	}

	//XXX Shader, MVP etc. uniforms

	/* Arcball camera rotates around region */
	mat4 view, proj, mvp;
	float aspect = window.width / (float)window.height;
	glm_perspective(glm_rad(client.camera.fov), aspect, 1, 2048, proj);
	glm_look(client.camera.position, client.camera.forward, opengl_up, view);
	glm_mat4_mulN((mat4 *[]){&proj, &view}, 2, mvp);

	glUseProgram(chunkmesh_renderer.shader);
	glUniformMatrix4fv(chunkmesh_renderer.uniforms.mvp, 1, GL_FALSE, (float *)mvp);

	/* Render chunks */
	size_t mesh_count = client.chunkmesh_map.entries_size;
	for (size_t i = 0; i < mesh_count; ++ i) {
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
