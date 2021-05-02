#include "hammer/appstate/viz_tectonic.h"
#include "hammer/appstate/viz_climate.h"
#include "hammer/appstate/world_config.h"
#include "hammer/cli.h"
#include "hammer/error.h"
#include "hammer/glthread.h"
#include "hammer/gui.h"
#include "hammer/math.h"
#include "hammer/mem.h"
#include "hammer/window.h"
#include "hammer/worldgen/tectonic.h"

#define PROGRESS_STR_MAX_LEN 64

struct viz_tectonic_appstate {
	dltask task;
	struct world_opts world_opts;
	struct tectonic_opts tectonic_opts;
	struct lithosphere lithosphere;
	gui_btn_state cancel_btn_state;
	GLuint tectonic_img;
	char progress_str[PROGRESS_STR_MAX_LEN];
};

static void viz_tectonic_entry(DL_TASK_ARGS);
static void viz_tectonic_exit (DL_TASK_ARGS);
static void viz_tectonic_loop (DL_TASK_ARGS);

static int viz_tectonic_gl_setup(void *);
static int viz_tectonic_gl_frame(void *);

static void blit_lithosphere_to_image(struct viz_tectonic_appstate *);

dltask *
viz_tectonic_appstate_alloc_detached(struct world_opts *world_opts)
{
	struct viz_tectonic_appstate *viz = xmalloc(sizeof(*viz));

	viz->task = DL_TASK_INIT(viz_tectonic_entry);
	viz->world_opts = *world_opts;
	/* TODO: Set in world_config */
	viz->tectonic_opts = (struct tectonic_opts) {
		TECTONIC_OPTS_DEFAULTS,
		.seed = world_opts->seed
	};

	return &viz->task;
}

static void
viz_tectonic_entry(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct viz_tectonic_appstate, viz, task);

	lithosphere_create(&viz->lithosphere, &viz->tectonic_opts);
	glthread_execute(viz_tectonic_gl_setup, viz);
	viz->cancel_btn_state = 0;

	dltail(&viz->task, viz_tectonic_loop);
}

static void
viz_tectonic_exit(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct viz_tectonic_appstate, viz, task);

	lithosphere_destroy(&viz->lithosphere);
	free(viz);
}

static void
viz_tectonic_loop(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct viz_tectonic_appstate, viz, task);

	size_t steps = viz->tectonic_opts.generations *
	                 viz->tectonic_opts.generation_steps;
	if (viz->lithosphere.generation > steps)
	{
		dltask *next = viz_climate_appstate_alloc_detached(
		                 &viz->world_opts, &viz->lithosphere);
		dlcontinuation(&viz->task, viz_tectonic_exit);
		dlwait(&viz->task, 1);
		dlnext(next, &viz->task);
		dlasync(next);
		return;
	}

	if (glthread_execute(viz_tectonic_gl_frame, viz) ||
	    viz->cancel_btn_state == GUI_BTN_RELEASED)
	{
		dltail(&viz->task, viz_tectonic_exit);
		return;
	}

	lithosphere_update(&viz->lithosphere, &viz->tectonic_opts);

	dltail(&viz->task, viz_tectonic_loop);
}

static int
viz_tectonic_gl_setup(void *viz_)
{
	struct viz_tectonic_appstate *viz = viz_;

	glClearColor(49 / 255.0f, 59 / 255.0f, 58 / 255.0f, 1);

	glGenTextures(1, &viz->tectonic_img);
	glBindTexture(GL_TEXTURE_2D, viz->tectonic_img);
	glTexImage2D(GL_TEXTURE_2D, 0, /* level */
	                            GL_RGB8,
	                            LITHOSPHERE_LEN, LITHOSPHERE_LEN,
	                            0, /* border */
	                            GL_RGB,
	                            GL_UNSIGNED_BYTE,
	                            NULL);
	glTextureParameteri(viz->tectonic_img, GL_TEXTURE_MAX_LEVEL, 0);
	glTextureParameteri(viz->tectonic_img, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(viz->tectonic_img, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteri(viz->tectonic_img, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(viz->tectonic_img, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glGenerateTextureMipmap(viz->tectonic_img);

	return 0;
}

static int
viz_tectonic_gl_frame(void *viz_)
{
	struct viz_tectonic_appstate *viz = viz_;

	if (window_startframe())
		return 1;

	blit_lithosphere_to_image(viz);

	unsigned font_size = 24;
	unsigned padding = 32;

	const char *title = "Tectonic Uplift";
	const char *cancel_btn_text = "Cancel";

	unsigned btn_height = font_size + 16;
	unsigned cancel_btn_width = window.width / 2;

	snprintf(viz->progress_str, PROGRESS_STR_MAX_LEN,
	         "Generation: %ld/%ld",
	         (long)viz->lithosphere.generation,
	         (long)viz->tectonic_opts.generations *
	               viz->tectonic_opts.generation_steps);

	struct text_opts title_opts = {
		TEXT_OPTS_DEFAULTS,
		.xoffset = padding,
		.yoffset = padding,
		.size    = font_size,
		.weight  = 255
	};

	struct text_opts progress_opts = {
		TEXT_OPTS_DEFAULTS,
		.xoffset = padding,
		.yoffset = window.height - font_size - padding,
		.size    = font_size
	};

	struct btn_opts cancel_btn_opts = {
		BTN_OPTS_DEFAULTS,
		.xoffset = window.width - cancel_btn_width - padding,
		.yoffset = window.height - btn_height - padding,
		.width   = cancel_btn_width,
		.height  = btn_height,
		.size    = font_size
	};

	float imgy = padding + font_size + 8;
	float img_height = window.height - imgy - btn_height - padding - 8;
	float dim = MIN(img_height, window.width - padding * 2);

	struct img_opts tectonic_img_opts = {
		IMG_OPTS_DEFAULTS,
		.xoffset = (window.width - dim) / 2,
		.yoffset = imgy,
		.zoffset = 1,
		.width  = dim,
		.height = dim
	};

	gui_text(title, strlen(title), title_opts);
	gui_text(viz->progress_str, strlen(viz->progress_str), progress_opts);

	viz->cancel_btn_state = gui_btn(
		viz->cancel_btn_state,
		cancel_btn_text, strlen(cancel_btn_text),
		cancel_btn_opts);

	gui_img(viz->tectonic_img, tectonic_img_opts);

	window_submitframe();

	return 0;
}

static void
blit_lithosphere_to_image(struct viz_tectonic_appstate *viz)
{
	struct lithosphere *l = &viz->lithosphere;
	GLubyte *img = xmalloc(LITHOSPHERE_AREA * sizeof(*img) * 3);
	/* Blue below sealevel, green to white continent altitude */
	for (size_t i = 0; i < LITHOSPHERE_AREA; ++ i) {
		if (l->mass[i] > TECTONIC_CONTINENT_MASS) {
			float h = l->mass[i] - TECTONIC_CONTINENT_MASS;
			img[i*3+0] = 30 + 95 * MIN(2,h) / 1.5f;
			img[i*3+1] = 125;
			img[i*3+2] = 30;
		} else {
			img[i*3+0] = 50;
			img[i*3+1] = 50 + 100 * l->mass[i] / TECTONIC_CONTINENT_MASS;
			img[i*3+2] = 168;
		}
	}
	/* Outline plates
	for (uint32_t y = 0; y < LITHOSPHERE_LEN; ++ y)
	for (uint32_t x = 0; x < LITHOSPHERE_LEN; ++ x) {
		size_t i  = y * LITHOSPHERE_LEN + x;
		size_t xm = y * LITHOSPHERE_LEN + wrap((long)x-1);
		size_t xp = y * LITHOSPHERE_LEN + wrap((long)x+1);
		size_t ym = wrap((long)y-1) * LITHOSPHERE_LEN + x;
		size_t yp = wrap((long)y+1) * LITHOSPHERE_LEN + x;
		if (l->owner[i] != (l->owner[xm] & l->owner[xp] &
				    l->owner[ym] & l->owner[yp]))
		{
			img[i*3+0] = 255;
			img[i*3+1] = 0;
			img[i*3+2] = 0;
		}
	}
	*/
	glTextureSubImage2D(viz->tectonic_img,
	                    0, /* level */
	                    0, 0, /* x,y offset */
	                    LITHOSPHERE_LEN, LITHOSPHERE_LEN, /* w,h */
	                    GL_RGB, GL_UNSIGNED_BYTE,
	                    img);
	free(img);
}
