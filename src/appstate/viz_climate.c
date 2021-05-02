#include "hammer/appstate/viz_climate.h"
#include "hammer/appstate/world_config.h"
#include "hammer/cli.h"
#include "hammer/error.h"
#include "hammer/glthread.h"
#include "hammer/gui.h"
#include "hammer/math.h"
#include "hammer/mem.h"
#include "hammer/window.h"
#include "hammer/worldgen/climate.h"
#include "hammer/worldgen/tectonic.h"

#define PROGRESS_STR_MAX_LEN 64

struct viz_climate_appstate {
	dltask task;
	struct world_opts opts;
	float *uplift;
	struct climate climate;
	gui_btn_state cancel_btn_state;
	GLuint biome_img;
	char progress_str[PROGRESS_STR_MAX_LEN];
};

static void viz_climate_entry(DL_TASK_ARGS);
static void viz_climate_exit (DL_TASK_ARGS);
static void viz_climate_loop (DL_TASK_ARGS);

static int viz_climate_gl_setup(void *);
static int viz_climate_gl_frame(void *);

static void blit_biome_to_image(struct viz_climate_appstate *);

dltask *
viz_climate_appstate_alloc_detached(struct world_opts *opts,
                                    struct lithosphere *l)
{
	struct viz_climate_appstate *viz = xmalloc(sizeof(*viz));

	viz->task = DL_TASK_INIT(viz_climate_entry);
	viz->opts = *opts;
	viz->uplift = xmalloc(opts->size * opts->size * sizeof(*viz->uplift));
	lithosphere_blit(l, viz->uplift, opts->size);
	climate_create(&viz->climate, viz->uplift, opts->size);

	return &viz->task;
}

static void
viz_climate_entry(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct viz_climate_appstate, viz, task);

	glthread_execute(viz_climate_gl_setup, viz);
	viz->cancel_btn_state = 0;

	dltail(&viz->task, viz_climate_loop);
}

static void
viz_climate_exit(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct viz_climate_appstate, viz, task);
	climate_destroy(&viz->climate);
	free(viz->uplift);
	free(viz);
}

static void
viz_climate_loop(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct viz_climate_appstate, viz, task);

	if (glthread_execute(viz_climate_gl_frame, viz) ||
	    viz->cancel_btn_state == GUI_BTN_RELEASED)
	{
		dltail(&viz->task, viz_climate_exit);
		return;
	}

	if (viz->climate.generation < CLIMATE_GENERATIONS) {
		climate_update(&viz->climate);
	} else {
		/* XXX Next step */
	}

	dltail(&viz->task, viz_climate_loop);
}

static int
viz_climate_gl_setup(void *viz_)
{
	struct viz_climate_appstate *viz = viz_;

	glClearColor(49 / 255.0f, 59 / 255.0f, 58 / 255.0f, 1);

	glGenTextures(1, &viz->biome_img);
	glBindTexture(GL_TEXTURE_2D, viz->biome_img);
	glTexImage2D(GL_TEXTURE_2D, 0, /* level */
	                            GL_RGB8,
	                            viz->climate.size, viz->climate.size,
	                            0, /* border */
	                            GL_RGB,
	                            GL_UNSIGNED_BYTE,
	                            NULL);
	glTextureParameteri(viz->biome_img, GL_TEXTURE_MAX_LEVEL, 0);
	glTextureParameteri(viz->biome_img, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(viz->biome_img, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteri(viz->biome_img, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(viz->biome_img, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glGenerateTextureMipmap(viz->biome_img);

	return 0;
}

static int
viz_climate_gl_frame(void *viz_)
{
	struct viz_climate_appstate *viz = viz_;

	if (window_startframe())
		return 1;

	blit_biome_to_image(viz);

	unsigned font_size = 24;
	unsigned padding = 32;

	const char *title = "Climate Generation";
	const char *cancel_btn_text = "Cancel";

	unsigned btn_height = font_size + 16;
	unsigned cancel_btn_width = window.width / 2;

	snprintf(viz->progress_str, PROGRESS_STR_MAX_LEN,
	         "Generation: %ld/%ld",
	         (long)viz->climate.generation,
	         (long)CLIMATE_GENERATIONS);

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

	struct img_opts biome_img_opts = {
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

	gui_img(viz->biome_img, biome_img_opts);

	window_submitframe();

	return 0;
}

static void
blit_biome_to_image(struct viz_climate_appstate *viz)
{
	GLubyte *img = xmalloc(viz->climate.size * viz->climate.size * sizeof(*img) * 3);
	/* Blue below sealevel, green to white continent altitude */
	const size_t area = viz->climate.size * viz->climate.size;
	for (size_t i = 0; i < area; ++ i) {
		float temperature = 1 - viz->climate.inv_temp[i];
		float freezegrad = 1 - CLAMP(2 * (temperature - CLIMATE_FREEZING_POINT), 0, 1);
		if (viz->uplift[i] > TECTONIC_CONTINENT_MASS) {
			float h = viz->uplift[i] - TECTONIC_CONTINENT_MASS;
			img[i*3+0] = 148 - 70 * MIN(1,viz->climate.precipitation[i]);
			img[i*3+1] = 148 - 20 * h;
			img[i*3+2] = 77;
			img[i*3+0] += (255 - img[i*3+0]) * freezegrad;
			img[i*3+1] += (255 - img[i*3+1]) * freezegrad;
			img[i*3+2] += (255 - img[i*3+2]) * freezegrad;
		} else {
			float temprg = (150-50) * freezegrad;
			float elevgrad = 100 * viz->uplift[i] / TECTONIC_CONTINENT_MASS;
			img[i*3+0] = 50 + temprg;
			img[i*3+1] = 50 + MAX(temprg,elevgrad);
			img[i*3+2] = 168 + (255-168) * freezegrad;
		}
	}
	glTextureSubImage2D(viz->biome_img,
	                    0, /* level */
	                    0, 0, /* x,y offset */
	                    viz->climate.size, viz->climate.size, /* w,h */
	                    GL_RGB, GL_UNSIGNED_BYTE,
	                    img);
	free(img);
}
