#include "hammer/appstate/region_generation.h"
#include "hammer/appstate/game.h"
#include "hammer/appstate/world_config.h"
#include "hammer/error.h"
#include "hammer/glsl.h"
#include "hammer/glthread.h"
#include "hammer/gui.h"
#include "hammer/hexagon.h"
#include "hammer/math.h"
#include "hammer/mem.h"
#include "hammer/window.h"
#include "hammer/worldgen/region.h"
#include <cglm/affine.h>
#include <cglm/cam.h>

/* TODO: Images never freed, mouse never potentially released */

struct region_renderer {
	GLuint shader;
	GLuint vao;
	GLuint vbo;
	GLuint heightmap_img;
	GLuint waterheight_img;
	struct {
		GLuint mvp;
		GLuint heightmap_sampler;
		GLuint waterheight_sampler;
		GLuint width;
		GLuint scale;
	} uniforms;
	size_t render_size;
	size_t render_verts;
};

struct region_generation_appstate {
	dltask                     task;
	const struct climate      *climate;
	const struct lithosphere  *lithosphere;
	const struct stream_graph *stream_graph;
	const struct world_opts   *world_opts;
	struct region              region;
	struct region_renderer     renderer;
	float                      yaw, pitch;
	unsigned                   stream_coord_left;
	unsigned                   stream_coord_top;
	unsigned                   stream_region_size;
	unsigned                   generations;
	int                        cancel_btn_state;
	int                        continue_btn_state;
	int                        mouse_captured;
};

static void region_generation_entry(DL_TASK_ARGS);
static void region_generation_exit(DL_TASK_ARGS);

static int region_generation_gl_create(void *);
static int region_generation_gl_destroy(void *);
static int region_generation_gl_frame(void *);

static void viz_region_loop(DL_TASK_ARGS);

static int region_generation_gl_blit_heightmap(void *);

dltask *
region_generation_appstate_alloc_detached(
	const struct climate *climate,
	const struct lithosphere *lithosphere,
	const struct stream_graph *stream_graph,
	const struct world_opts *world_opts,
	unsigned stream_coord_left,
	unsigned stream_coord_top,
	unsigned stream_region_size)
{
	struct region_generation_appstate *rg = xmalloc(sizeof(*rg));
	rg->task         = DL_TASK_INIT(region_generation_entry);
	rg->climate      = climate;
	rg->lithosphere  = lithosphere;
	rg->stream_graph = stream_graph;
	rg->world_opts   = world_opts;
	memset(&rg->region, 0, sizeof(rg->region));
	rg->stream_coord_left = stream_coord_left;
	rg->stream_coord_top = stream_coord_top;
	rg->stream_region_size = stream_region_size;
	return &rg->task;
}

static void
region_generation_entry(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct region_generation_appstate, rg, task);
	rg->generations = 0;
	rg->cancel_btn_state = 0;
	rg->continue_btn_state = 0;
	rg->mouse_captured = 0;
	region_create(&rg->region,
	              rg->stream_coord_left,
	              rg->stream_coord_top,
	              rg->stream_region_size,
	              rg->stream_graph);
	rg->yaw = -M_PI / 2;
	rg->pitch = -FLT_EPSILON;
	glthread_execute(region_generation_gl_create, rg);
	dltail(&rg->task, viz_region_loop);
}

static void
region_generation_exit(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct region_generation_appstate, rg, task);
	glthread_execute(region_generation_gl_destroy, rg);
	region_destroy(&rg->region);
	free(rg);
}

static void
viz_region_loop(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct region_generation_appstate, rg, task);

	if (glthread_execute(region_generation_gl_frame, rg) ||
	    rg->cancel_btn_state == GUI_BTN_RELEASED)
	{
		dltail(&rg->task, region_generation_exit);
		return;
	}

	if (rg->continue_btn_state == GUI_BTN_RELEASED) {
		/* Kick off the game! */
		dltask *next = game_appstate_alloc_detached();
		dlcontinuation(&rg->task, region_generation_exit);
		dlwait(&rg->task, 1);
		dlnext(next, &rg->task);
		dlasync(next);
		return;
	}

	if (rg->generations < REGION_GENERATIONS) {
		++ rg->generations;
		region_erode(&rg->region);
		glthread_execute(region_generation_gl_blit_heightmap, rg);
	}

	dltail(&rg->task, viz_region_loop);
}

static int
region_generation_gl_create(void *rg_)
{
	struct region_generation_appstate *rg = rg_;
	struct region_renderer *renderer = &rg->renderer;

	renderer->render_size = 512;
	renderer->shader = compile_shader_program(
	                     "resources/shaders/region.vs",
	                     "resources/shaders/region.gs",
	                     "resources/shaders/region.fs");
	if (renderer->shader == 0)
		xpanic("Error creating region shader");
	glUseProgram(renderer->shader);

	renderer->uniforms.mvp = glGetUniformLocation(renderer->shader, "mvp");
	renderer->uniforms.heightmap_sampler = glGetUniformLocation(renderer->shader, "heightmap_sampler");
	renderer->uniforms.waterheight_sampler = glGetUniformLocation(renderer->shader, "waterheight_sampler");
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

	glGenTextures(1, &renderer->heightmap_img);
	glBindTexture(GL_TEXTURE_2D, renderer->heightmap_img);
	glTexImage2D(GL_TEXTURE_2D, 0, /* level */
	                            GL_R32F,
	                            rg->region.rect_size, rg->region.rect_size,
	                            0, /* border */
	                            GL_RED,
	                            GL_FLOAT,
	                            NULL);
	glTextureParameteri(renderer->heightmap_img, GL_TEXTURE_MAX_LEVEL, 0);
	glTextureParameteri(renderer->heightmap_img, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(renderer->heightmap_img, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteri(renderer->heightmap_img, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTextureParameteri(renderer->heightmap_img, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glGenerateTextureMipmap(renderer->heightmap_img);

	glGenTextures(1, &renderer->waterheight_img);
	glBindTexture(GL_TEXTURE_2D, renderer->waterheight_img);
	glTexImage2D(GL_TEXTURE_2D, 0, /* level */
	                            GL_R32F,
	                            rg->region.rect_size, rg->region.rect_size,
	                            0, /* border */
	                            GL_RED,
	                            GL_FLOAT,
	                            NULL);
	glTextureParameteri(renderer->waterheight_img, GL_TEXTURE_MAX_LEVEL, 0);
	glTextureParameteri(renderer->waterheight_img, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(renderer->waterheight_img, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteri(renderer->waterheight_img, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTextureParameteri(renderer->waterheight_img, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glGenerateTextureMipmap(renderer->waterheight_img);

	/* Constant uniform values */
	glUniform1f(renderer->uniforms.width, renderer->render_size);
	glUniform1f(renderer->uniforms.scale, renderer->render_size / (float)rg->region.rect_size);
	glUniform1i(renderer->uniforms.heightmap_sampler, 0);
	glUniform1i(renderer->uniforms.waterheight_sampler, 1);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, renderer->heightmap_img);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, renderer->waterheight_img);

	glClearColor(49 / 255.0f, 59 / 255.0f, 58 / 255.0f, 1);

	return 0;
}

static int
region_generation_gl_destroy(void *rg_)
{
	struct region_generation_appstate *rg = rg_;

	glDeleteVertexArrays(1, &rg->renderer.vao);
	glDeleteProgram(rg->renderer.shader);

	return 0;
}

static int
region_generation_gl_frame(void *rg_)
{
	struct region_generation_appstate *rg = rg_;

	if (rg->mouse_captured) {
		if (!window.mouse_held[MOUSEBM]) {
			rg->mouse_captured = 0;
			window_unlock_mouse();
		}
		rg->yaw   += window.motion_x / 100.0f;
		rg->pitch += window.motion_y / 100.0f;
		rg->pitch = CLAMP(rg->pitch, -M_PI/2, -FLT_EPSILON);
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
	         rg->world_opts->seed,
	         rg->region.hex_size,
	         rg->region.stream_coord_left,
	         rg->region.stream_coord_top);
	gui_text(region_label, normal_text_opts);
	gui_stack_break(&stack);

	rg->cancel_btn_state = gui_btn(rg->cancel_btn_state, "Cancel", btn_opts);
	gui_stack_break(&stack);

	if (rg->generations >= REGION_GENERATIONS) {
		rg->continue_btn_state = gui_btn(rg->continue_btn_state, "Continue", btn_opts);
		gui_stack_break(&stack);
	}

	/* Handle after UI has a chance to steal mouse event */
	if (window.unhandled_mouse_press[MOUSEBM]) {
		window.unhandled_mouse_press[MOUSEBM] = 0;
		rg->mouse_captured = 1;
		window_lock_mouse();
	}

	const float r = rg->renderer.render_size;
	mat4 model, view, proj, mvp;
	glm_translate_make(model, (vec3) { -r / 2.0f, 0, -r / 2.0f });
	float aspect = window.width / (float)window.height;
	glm_perspective(glm_rad(60), aspect, 1, 1000, proj);
	glm_lookat((vec3){r * sinf(rg->pitch) * cosf(rg->yaw),
	                  r * cosf(rg->pitch),
	                  r * sinf(rg->pitch) * sinf(rg->yaw)},
	           (vec3){0, 0, 0},
	           (vec3){0, 1, 0},
	           view);
	glm_mat4_mulN((mat4 *[]){&proj, &view, &model}, 3, mvp);

	glUseProgram(rg->renderer.shader);
	glBindVertexArray(rg->renderer.vao);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, rg->renderer.heightmap_img);
	glUniformMatrix4fv(rg->renderer.uniforms.mvp, 1, GL_FALSE, (float *)mvp);
	glBindBuffer(GL_ARRAY_BUFFER, rg->renderer.vbo);
	glDrawArrays(GL_TRIANGLES, 0, rg->renderer.render_verts);

	gui_container_pop();
	window_submitframe();
	return window.should_close;
}

static int
region_generation_gl_blit_heightmap(void *rg_)
{
	struct region_generation_appstate *rg = rg_;

	glTextureSubImage2D(rg->renderer.heightmap_img,
	                    0, /* level */
	                    0, 0, /* x,y offset */
	                    rg->region.rect_size, rg->region.rect_size, /* w,h */
	                    GL_RED, GL_FLOAT,
	                    rg->region.height);

	glTextureSubImage2D(rg->renderer.waterheight_img,
	                    0, /* level */
	                    0, 0, /* x,y offset */
	                    rg->region.rect_size, rg->region.rect_size, /* w,h */
	                    GL_RED, GL_FLOAT,
	                    rg->region.water);

	return 0;
}
