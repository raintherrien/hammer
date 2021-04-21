#include "hammer/appstate/visualize_tectonic.h"
#include "hammer/appstate/manager.h"
#include "hammer/error.h"
#include "hammer/glthread.h"
#include "hammer/math.h"
#include "hammer/mem.h"
#include "hammer/window.h"

static void visualize_tectonic_entry(void *);
static void visualize_tectonic_exit(void *);
static void visualize_tectonic_loop(void *, struct appstate_cmd *);

static int visualize_tectonic_gl_setup(void *);
static int visualize_tectonic_gl_frame(void *);

static void blit_lithosphere_to_image(struct appstate_visualize_tectonic *);

struct appstate
appstate_visualize_tectonic_alloc(struct rtargs args)
{
	struct appstate_visualize_tectonic *state = xmalloc(sizeof(*state));

	state->args = args;
	/* TODO: Set in world_config */
	state->tectonic_opts = (struct tectonic_opts) {
		TECTONIC_OPTS_DEFAULTS,
		.seed = args.seed
	};

	return (struct appstate) {
		.entry_fn = visualize_tectonic_entry,
		.exit_fn  = visualize_tectonic_exit,
		.loop_fn  = visualize_tectonic_loop,
		.arg      = state
	};
}

static void
visualize_tectonic_entry(void *state_)
{
	struct appstate_visualize_tectonic *state = state_;
	lithosphere_create(&state->lithosphere, &state->tectonic_opts);
	glthread_execute(visualize_tectonic_gl_setup, state);
	state->cancel_btn_state = 0;
}

static void
visualize_tectonic_exit(void *state_)
{
	struct appstate_visualize_tectonic *state = state_;
	lithosphere_destroy(&state->lithosphere);
}

static void
visualize_tectonic_loop(void *state_, struct appstate_cmd *cmd)
{
	struct appstate_visualize_tectonic *state = state_;

	size_t steps = state->tectonic_opts.generations *
	                 state->tectonic_opts.generation_steps;
	if (state->lithosphere.generation > steps)
	{
		/* TODO: Next step */
	}

	lithosphere_update(&state->lithosphere, &state->tectonic_opts);

	if (glthread_execute(visualize_tectonic_gl_frame, state))
		cmd->type = APPSTATE_CMD_POP;

	if (state->cancel_btn_state == GUI_BTN_RELEASED)
		cmd->type = APPSTATE_CMD_POP;
}

static int
visualize_tectonic_gl_setup(void *state_)
{
	struct appstate_visualize_tectonic *state = state_;

	glClearColor(49 / 255.0f, 59 / 255.0f, 58 / 255.0f, 1);

	glGenTextures(1, &state->tectonic_img);
	glBindTexture(GL_TEXTURE_2D, state->tectonic_img);
	glTexImage2D(GL_TEXTURE_2D, 0, /* level */
	                            GL_RGB8,
	                            LITHOSPHERE_LEN, LITHOSPHERE_LEN,
	                            0, /* border */
	                            GL_RGB,
	                            GL_UNSIGNED_BYTE,
	                            NULL);
	glTextureParameteri(state->tectonic_img, GL_TEXTURE_MAX_LEVEL, 0);
	glTextureParameteri(state->tectonic_img, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(state->tectonic_img, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteri(state->tectonic_img, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(state->tectonic_img, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glGenerateTextureMipmap(state->tectonic_img);

	return 0;
}

static int
visualize_tectonic_gl_frame(void *state_)
{
	struct appstate_visualize_tectonic *state = state_;

	if (window_startframe())
		return 1;

	blit_lithosphere_to_image(state);

	unsigned font_size = 24;
	unsigned padding = 32;

	const char *title = "Tectonic Uplift";
	const char *cancel_btn_text = "Cancel";

	unsigned btn_height = font_size + 16;
	unsigned cancel_btn_width = window.width / 2;

	snprintf(state->progress_str, PROGRESS_STR_MAX_LEN,
	         "Generation: %ld/%ld",
	         (long)state->lithosphere.generation,
	         (long)state->tectonic_opts.generations *
	               state->tectonic_opts.generation_steps);

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
	gui_text(state->progress_str, strlen(state->progress_str), progress_opts);

	state->cancel_btn_state = gui_btn(
		state->cancel_btn_state,
		cancel_btn_text, strlen(cancel_btn_text),
		cancel_btn_opts);

	gui_img(state->tectonic_img, tectonic_img_opts);

	window_submitframe();

	return 0;
}

static void
blit_lithosphere_to_image(struct appstate_visualize_tectonic *state)
{
	struct lithosphere *l = &state->lithosphere;
	GLubyte *img = xmalloc(LITHOSPHERE_AREA * sizeof(*img) * 3);
	/* Blue below sealevel, green to white continent altitude */
	for (size_t i = 0; i < LITHOSPHERE_AREA; ++ i) {
		if (l->mass[i] > TECTONIC_CONTINENT_MASS) {
			float h = l->mass[i] - TECTONIC_CONTINENT_MASS;
			img[i*3+2] = 255 * (0.235f + 0.265f / 2 * MIN(2,h));
			img[i*3+1] = 128;
			img[i*3+0] = 255 * (0.235f + 0.265f / 2 * MIN(2,h));
		} else {
			img[i*3+2] = 255 * l->mass[i] / TECTONIC_CONTINENT_MASS;
			img[i*3+1] = 0;
			img[i*3+0] = 0;
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
	glTextureSubImage2D(state->tectonic_img,
	                    0, /* level */
	                    0, 0, /* x,y offset */
	                    LITHOSPHERE_LEN, LITHOSPHERE_LEN, /* w,h */
	                    GL_RGB, GL_UNSIGNED_BYTE,
	                    img);
	free(img);
}
