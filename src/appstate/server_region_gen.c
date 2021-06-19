#include "hammer/appstate.h"
#include "hammer/error.h"
#include "hammer/glsl.h"
#include "hammer/glthread.h"
#include "hammer/hexagon.h"
#include "hammer/math.h"
#include "hammer/mem.h"
#include "hammer/server.h"
#include "hammer/window.h"
#include <cglm/affine.h>
#include <cglm/cam.h>

/*
 * TODO: Allow the user to select an exact embark location by left clicking
 * the region and raycasting into the terrain. Put a red inverted pyramid
 * there and make that the center of the camera's focus
 */

dltask appstate_server_region_gen_frame;

enum {
	HEIGHTMAP_IMG_SEDIMENT,
	HEIGHTMAP_IMG_STONE,
	HEIGHTMAP_IMG_WATER,
	HEIGHTMAP_IMG_COUNT
};

struct region_renderer {
	GLuint shader;
	GLuint vao;
	GLuint vbo;
	GLuint heightmap_imgs[HEIGHTMAP_IMG_COUNT];
	struct {
		GLuint mvp;
		GLuint sediment_sampler;
		GLuint stone_sampler;
		GLuint water_sampler;
		GLuint width;
		GLuint scale;
	} uniforms;
	size_t render_size;
	size_t render_verts;
};

static struct {
	struct region_renderer renderer;
	unsigned generations;
	float    yaw;
	float    pitch;
	float    zoom;
	int      cancel_btn_state;
	int      continue_btn_state;
	int      mouse_captured;
} server_region_gen;

static int region_generation_gl_create(void *);
static int region_generation_gl_destroy(void *);
static int region_generation_gl_frame(void *);
static void region_generation_frame_async(DL_TASK_ARGS);
static int region_generation_gl_blit_heightmap(void *);

void
appstate_server_region_gen_setup(void)
{
	appstate_server_region_gen_frame = DL_TASK_INIT(region_generation_frame_async);

	region_create(&server.world.region,
	              server.world.region_stream_coord_left,
	              server.world.region_stream_coord_top,
	              STREAM_REGION_SIZE_MIN * (1 << server.world.region_size_mag2),
	              server.planet.stream);

	server_region_gen.generations = 0;
	server_region_gen.yaw = -M_PI / 2;
	server_region_gen.pitch = -FLT_EPSILON;
	server_region_gen.zoom = 1;
	server_region_gen.cancel_btn_state = 0;
	server_region_gen.continue_btn_state = 0;
	server_region_gen.mouse_captured = 0;

	glthread_execute(region_generation_gl_create, NULL);
}

void
appstate_server_region_gen_teardown_confirm_region(void)
{
	/* TODO: Images never freed, mouse never potentially released */

	glthread_execute(region_generation_gl_destroy, NULL);
}

void
appstate_server_region_gen_teardown_discard_region(void)
{
	/* TODO: Images never freed, mouse never potentially released */

	glthread_execute(region_generation_gl_destroy, NULL);
	region_destroy(&server.world.region);
}

static void
region_generation_frame_async(DL_TASK_ARGS)
{
	DL_TASK_ENTRY_VOID;

	if (glthread_execute(region_generation_gl_frame, NULL) ||
	    server_region_gen.cancel_btn_state == GUI_BTN_RELEASED)
	{
                appstate_transition(APPSTATE_TRANSITION_SERVER_REGION_GEN_CANCEL);
		return;
	}

	if (server_region_gen.continue_btn_state == GUI_BTN_RELEASED) {
		/* Kick off the game! */
                appstate_transition(APPSTATE_TRANSITION_CONFIRM_REGION);
		return;
	}

	if (server_region_gen.generations < REGION_GENERATIONS) {
		++ server_region_gen.generations;
		region_erode(&server.world.region);
		glthread_execute(region_generation_gl_blit_heightmap, NULL);
	}
}

static int
region_generation_gl_create(void *_)
{
	struct region_renderer *renderer = &server_region_gen.renderer;

	renderer->render_size = 512;
	renderer->shader = compile_shader_program(
	                     "resources/shaders/region.vs",
	                     "resources/shaders/region.gs",
	                     "resources/shaders/region.fs");
	if (renderer->shader == 0)
		xpanic("Error creating region shader");
	glUseProgram(renderer->shader);

	renderer->uniforms.mvp = glGetUniformLocation(renderer->shader, "mvp");
	renderer->uniforms.sediment_sampler = glGetUniformLocation(renderer->shader, "sediment_sampler");
	renderer->uniforms.stone_sampler = glGetUniformLocation(renderer->shader, "stone_sampler");
	renderer->uniforms.water_sampler = glGetUniformLocation(renderer->shader, "water_sampler");
	renderer->uniforms.width = glGetUniformLocation(renderer->shader, "width");
	renderer->uniforms.scale = glGetUniformLocation(renderer->shader, "scale");

	size_t max_verts = (renderer->render_size+1) *
	                   (renderer->render_size+1) * 6;
	GLfloat *vertices = xcalloc(max_verts * 2, sizeof(*vertices));

	renderer->render_verts = 0;
	for (float y = 0; y <= renderer->render_size; ++ y)
	for (float x = 0; x <= renderer->render_size; ++ x) {
		/* Crudely emulate the hexagonal region shape */
		float approx_region_hex_size = renderer->render_size / 2.0f;
		float approx_cell_hex_size = 0.577f; /* sqrtf(3) / 3 */
		float q, r;
		/*
		 * XXX Add width lost to hexagon width/height ratio to keep
		 * our region centered on whatever land features the user
		 * selected.
		 */
		float xxxoffset = (renderer->render_size - approx_region_hex_size * sqrtf(3)) / 2;
		hex_pixel_to_axial(approx_cell_hex_size * sqrtf(3),
		                   approx_cell_hex_size,
		                   x - renderer->render_size / 2.0f,
		                   y - renderer->render_size / 2.0f + xxxoffset,
		                   &q, &r);
		float d = MAX(MAX(fabsf(q), fabsf(r)), fabsf(-q-r));
		if (d > approx_region_hex_size)
			continue;

		GLfloat tris[6][2] = {
			{ x+0, y+0 },
			{ x+0, y+1 },
			{ x+1, y+1 },

			{ x+1, y+1 },
			{ x+1, y+0 },
			{ x+0, y+0 }
		};
		memcpy(vertices + renderer->render_verts * 2, tris, 2 * 6 * sizeof(*vertices));
		renderer->render_verts += 6;
	}

	glGenVertexArrays(1, &renderer->vao);
	glBindVertexArray(renderer->vao);
	glGenBuffers(1, &renderer->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(*vertices)*2, NULL);
	glBufferData(GL_ARRAY_BUFFER,
	             renderer->render_verts * 2 * sizeof(*vertices),
	             vertices,
	             GL_STATIC_DRAW);

	free(vertices);

	glGenTextures(3, renderer->heightmap_imgs);
	for (size_t i = 0; i < 3; ++ i) {
		GLuint img = renderer->heightmap_imgs[i];
		glBindTexture(GL_TEXTURE_2D, img);
		glTexImage2D(GL_TEXTURE_2D,
		             0, /* level */
		             GL_R32F,
		             server.world.region.rect_size, server.world.region.rect_size,
		             0, /* border */
		             GL_RED,
		             GL_FLOAT,
		             NULL);
		glTextureParameteri(img, GL_TEXTURE_MAX_LEVEL, 0);
		glTextureParameteri(img, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(img, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(img, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTextureParameteri(img, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glGenerateTextureMipmap(img);
	}

	/* Constant uniform values */
	glUniform1f(renderer->uniforms.width, renderer->render_size);
	glUniform1f(renderer->uniforms.scale, renderer->render_size / (float)server.world.region.rect_size);
	glUniform1i(renderer->uniforms.sediment_sampler, 0);
	glUniform1i(renderer->uniforms.stone_sampler, 1);
	glUniform1i(renderer->uniforms.water_sampler, 2);
	for (size_t i = 0; i < 3; ++ i) {
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, renderer->heightmap_imgs[i]);
	}

	glClearColor(49 / 255.0f, 59 / 255.0f, 58 / 255.0f, 1);

	return 0;
}

static int
region_generation_gl_destroy(void *_)
{
	glDeleteVertexArrays(1, &server_region_gen.renderer.vao);
	glDeleteProgram(server_region_gen.renderer.shader);

	return 0;
}

static int
region_generation_gl_frame(void *_)
{
	if (window.scroll)
	{
		server_region_gen.zoom += window.scroll / 40.0f;
		server_region_gen.zoom = CLAMP(server_region_gen.zoom, 1, 3);
	}

	if (server_region_gen.mouse_captured) {
		if (!window.mouse_held[MOUSEBM]) {
			server_region_gen.mouse_captured = 0;
			window_unlock_mouse();
		}
		server_region_gen.yaw   += window.motion_x / 100.0f;
		server_region_gen.pitch += window.motion_y / 100.0f;
		server_region_gen.pitch = CLAMP(server_region_gen.pitch, -M_PI/2, -FLT_EPSILON);
	}

	unsigned font_size = 24;
	unsigned border_padding = 32;
	unsigned element_padding = 8;

	const struct text_opts bold_text_opts = {
		TEXT_OPTS_DEFAULTS,
		.size = font_size,
		.weight = 255
	};

	const struct text_opts normal_text_opts = {
		TEXT_OPTS_DEFAULTS,
		.size = font_size
	};

	struct btn_opts btn_opts = {
		BTN_OPTS_DEFAULTS,
		.width = font_size * 8,
		.height = font_size + 16,
		.size = font_size
	};

	gui_container stack;
	gui_stack_init(&stack, (struct stack_opts) {
		STACK_OPTS_DEFAULTS,
		.vpadding = element_padding,
		.xoffset = border_padding,
		.yoffset = border_padding,
		.zoffset = 1
	});
	gui_container_push(&stack);

	gui_text("Region view", bold_text_opts);
	gui_stack_break(&stack);
	char region_label[128];
	snprintf(region_label, 128, "Seed: %llu, %zu @ (%u,%u)",
	         server.world.opts.seed,
	         server.world.region.hex_size,
	         server.world.region.stream_coord_left,
	         server.world.region.stream_coord_top);
	gui_text(region_label, normal_text_opts);
	gui_stack_break(&stack);

	server_region_gen.cancel_btn_state = gui_btn(server_region_gen.cancel_btn_state, "Cancel", btn_opts);
	gui_stack_break(&stack);

	if (server_region_gen.generations >= REGION_GENERATIONS) {
		server_region_gen.continue_btn_state = gui_btn(server_region_gen.continue_btn_state, "Continue", btn_opts);
		gui_stack_break(&stack);
	}

	/* Handle after UI has a chance to steal mouse event */
	if (window.unhandled_mouse_press[MOUSEBM]) {
		window.unhandled_mouse_press[MOUSEBM] = 0;
		server_region_gen.mouse_captured = 1;
		window_lock_mouse();
	}

	/* Arcball camera rotates around region */
	const float r = server_region_gen.renderer.render_size;
	const float fov = glm_rad(60) / server_region_gen.zoom;
	mat4 model, view, proj, mvp;
	glm_translate_make(model, (vec3) { -r / 2.0f, 0, -r / 2.0f });
	float aspect = window.width / (float)window.height;
	glm_perspective(fov, aspect, 1, 1000, proj);
	glm_lookat((vec3){r * sinf(server_region_gen.pitch) * cosf(server_region_gen.yaw),
	                  r * cosf(server_region_gen.pitch),
	                  r * sinf(server_region_gen.pitch) * sinf(server_region_gen.yaw)},
	           (vec3){0, 0, 0},
	           (vec3){0, 1, 0},
	           view);
	glm_mat4_mulN((mat4 *[]){&proj, &view, &model}, 3, mvp);

	glUseProgram(server_region_gen.renderer.shader);
	glBindVertexArray(server_region_gen.renderer.vao);
	glUniformMatrix4fv(server_region_gen.renderer.uniforms.mvp, 1, GL_FALSE, (float *)mvp);
	glBindBuffer(GL_ARRAY_BUFFER, server_region_gen.renderer.vbo);
	glDrawArrays(GL_TRIANGLES, 0, server_region_gen.renderer.render_verts);

	gui_container_pop();
	window_submitframe();
	return window.should_close;
}

static int
region_generation_gl_blit_heightmap(void *_)
{
	glTextureSubImage2D(server_region_gen.renderer.heightmap_imgs[HEIGHTMAP_IMG_SEDIMENT],
	                    0, /* level */
	                    0, 0, /* x,y offset */
	                    server.world.region.rect_size, server.world.region.rect_size, /* w,h */
	                    GL_RED, GL_FLOAT,
	                    server.world.region.sediment);

	glTextureSubImage2D(server_region_gen.renderer.heightmap_imgs[HEIGHTMAP_IMG_STONE],
	                    0, /* level */
	                    0, 0, /* x,y offset */
	                    server.world.region.rect_size, server.world.region.rect_size, /* w,h */
	                    GL_RED, GL_FLOAT,
	                    server.world.region.stone);

	glTextureSubImage2D(server_region_gen.renderer.heightmap_imgs[HEIGHTMAP_IMG_WATER],
	                    0, /* level */
	                    0, 0, /* x,y offset */
	                    server.world.region.rect_size, server.world.region.rect_size, /* w,h */
	                    GL_RED, GL_FLOAT,
	                    server.world.region.water);

	return 0;
}
