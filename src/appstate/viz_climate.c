#include "hammer/appstate/viz_climate.h"
#include "hammer/appstate/world_config.h"
#include "hammer/cli.h"
#include "hammer/error.h"
#include "hammer/glthread.h"
#include "hammer/gui.h"
#include "hammer/math.h"
#include "hammer/mem.h"
#include "hammer/window.h"
#include "hammer/worldgen/biome.h"
#include "hammer/worldgen/climate.h"
#include "hammer/worldgen/tectonic.h"

#define PROGRESS_STR_MAX_LEN 64
#define BIOME_TOOLTIP_STR_MAX_LEN 64

struct viz_climate_appstate {
	dltask task;
	struct world_opts opts;
	struct climate climate;
	gui_btn_state cancel_btn_state;
	int show_biomes;
	GLuint biome_img;
	char progress_str[PROGRESS_STR_MAX_LEN];
	char biome_tooltip_str[BIOME_TOOLTIP_STR_MAX_LEN];
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
	climate_create(&viz->climate, l);

	return &viz->task;
}

static void
viz_climate_entry(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct viz_climate_appstate, viz, task);

	glthread_execute(viz_climate_gl_setup, viz);
	viz->cancel_btn_state = 0;
	viz->show_biomes = 1;

	dltail(&viz->task, viz_climate_loop);
}

static void
viz_climate_exit(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct viz_climate_appstate, viz, task);
	climate_destroy(&viz->climate);
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
	                            CLIMATE_LEN, CLIMATE_LEN,
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

	window_startframe();

	blit_biome_to_image(viz);

	unsigned font_size = 24;
	unsigned padding = 32;

	const char *title = "Climate Generation";
	const char *cancel_btn_text = "Cancel";
	const char *show_biomes_text = "Show biomes";

	unsigned btn_height = font_size + 16;
	unsigned cancel_btn_width = window.width / 2;

	unsigned check_size = font_size + 2;

	snprintf(viz->progress_str,
	         PROGRESS_STR_MAX_LEN,
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
		.yoffset = padding + font_size,
		.size    = font_size
	};

	struct check_opts show_biomes_check_opts = {
		CHECK_OPTS_DEFAULTS,
		.xoffset = padding,
		.yoffset = padding + font_size * 2,
		.size    = font_size,
		.width   = check_size,
		.height  = check_size
	};

	struct text_opts show_biomes_text_opts = {
		TEXT_OPTS_DEFAULTS,
		.xoffset = padding + show_biomes_check_opts.width + 4,
		.yoffset = padding + font_size * 2,
		.size    = font_size
	};

	struct text_opts biome_tooltip_opts = {
		TEXT_OPTS_DEFAULTS,
		.xoffset = padding,
		.yoffset = padding + font_size * 3,
		.size    = font_size
	};

	float imgy = title_opts.yoffset;
	float img_height = window.height - imgy - btn_height - padding - 8;
	float dim = MIN(img_height, cancel_btn_width);

	struct img_opts biome_img_opts = {
		IMG_OPTS_DEFAULTS,
		.xoffset = window.width - padding - dim,
		.yoffset = imgy,
		.zoffset = 1,
		.width  = dim,
		.height = dim
	};

	struct btn_opts cancel_btn_opts = {
		BTN_OPTS_DEFAULTS,
		.xoffset = biome_img_opts.xoffset,
		.yoffset = imgy + dim + padding,
		.width   = dim,
		.height  = btn_height,
		.size    = font_size
	};

	if (window.mouse_x >= biome_img_opts.xoffset &&
	    window.mouse_x < biome_img_opts.xoffset + biome_img_opts.width &&
            window.mouse_y >= biome_img_opts.yoffset &&
            window.mouse_y < biome_img_opts.yoffset + biome_img_opts.height)
        {
		float s = CLIMATE_LEN / dim;
		size_t imgx = s * (window.mouse_x - biome_img_opts.xoffset);
		size_t imgy = s * (window.mouse_y - biome_img_opts.yoffset);
		size_t i = imgy * CLIMATE_LEN + imgx;
		float temp = 1 - viz->climate.inv_temp[i];
		float precip = viz->climate.precipitation[i];
		if (viz->climate.uplift[i] < TECTONIC_CONTINENT_MASS)
			precip = -1;
		enum biome b = biome_classification(temp, precip);
		snprintf(viz->biome_tooltip_str,
		         BIOME_TOOLTIP_STR_MAX_LEN,
		         "Hovering over: %s",
		         biome_name[b]);
	} else {
		snprintf(viz->biome_tooltip_str,
		         BIOME_TOOLTIP_STR_MAX_LEN,
		         "Hover over map to see biome");
	}

	gui_text(title, strlen(title), title_opts);
	gui_text(viz->progress_str, strlen(viz->progress_str), progress_opts);
	gui_text(viz->biome_tooltip_str, strlen(viz->biome_tooltip_str), biome_tooltip_opts);

	viz->show_biomes = gui_check(viz->show_biomes,
	                             show_biomes_check_opts);
	gui_text(show_biomes_text,
	         strlen(show_biomes_text),
	         show_biomes_text_opts);

	viz->cancel_btn_state = gui_btn(viz->cancel_btn_state,
	                                cancel_btn_text,
	                                strlen(cancel_btn_text),
	                                cancel_btn_opts);

	gui_img(viz->biome_img, biome_img_opts);

	window_submitframe();

	return window.should_close;
}

static void
blit_biome_to_image(struct viz_climate_appstate *viz)
{
	GLubyte *img = xmalloc(CLIMATE_AREA * sizeof(*img) * 3);
	/* Blue below sealevel, green to white continent altitude */
	if (viz->show_biomes) {
		for (size_t i = 0; i < CLIMATE_AREA; ++ i) {
			float temp = 1 - viz->climate.inv_temp[i];
			float precip = viz->climate.precipitation[i];
			if (viz->climate.uplift[i] < TECTONIC_CONTINENT_MASS)
				precip = -1;
			enum biome b = biome_classification(temp, precip);
			memcpy(&img[i*3], biome_color[b], sizeof(GLubyte)*3);
		}
	} else {
		for (size_t i = 0; i < CLIMATE_AREA; ++ i) {
			float temperature = 1 - viz->climate.inv_temp[i];
			float freezegrad = 1 - CLAMP(2 * (temperature - CLIMATE_ARCTIC_MAX_TEMP), 0, 1);
			if (viz->climate.uplift[i] > TECTONIC_CONTINENT_MASS) {
				float h = viz->climate.uplift[i] - TECTONIC_CONTINENT_MASS;
				img[i*3+0] = 148 - 70 * MIN(1,viz->climate.precipitation[i]);
				img[i*3+1] = 148 - 20 * h;
				img[i*3+2] = 77;
				img[i*3+0] += (255 - img[i*3+0]) * freezegrad;
				img[i*3+1] += (255 - img[i*3+1]) * freezegrad;
				img[i*3+2] += (255 - img[i*3+2]) * freezegrad;
			} else {
				float temprg = (150-50) * freezegrad;
				float elevgrad = 100 * viz->climate.uplift[i] / TECTONIC_CONTINENT_MASS;
				img[i*3+0] = 50 + temprg;
				img[i*3+1] = 50 + MAX(temprg,elevgrad);
				img[i*3+2] = 168 + (255-168) * freezegrad;
			}
		}
	}
	glTextureSubImage2D(viz->biome_img,
	                    0, /* level */
	                    0, 0, /* x,y offset */
	                    CLIMATE_LEN, CLIMATE_LEN, /* w,h */
	                    GL_RGB, GL_UNSIGNED_BYTE,
	                    img);
	free(img);
}
