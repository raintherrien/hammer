#include "hammer/appstate/world_config.h"
#include "hammer/appstate/viz_tectonic.h"
#include "hammer/cli.h"
#include "hammer/error.h"
#include "hammer/glthread.h"
#include "hammer/mem.h"
#include "hammer/version.h"
#include "hammer/window.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <time.h>

#define WORLDCONF_NUM_EDIT_BUFFER_LEN 32

struct world_config_appstate {
	dltask task;
	struct world_opts opts;
	gui_btn_state next_btn_state;
	gui_btn_state exit_btn_state;
	char size_edit_buf[WORLDCONF_NUM_EDIT_BUFFER_LEN];
	char seed_edit_buf[WORLDCONF_NUM_EDIT_BUFFER_LEN];
};

static void world_config_entry(DL_TASK_ARGS);
static void world_config_loop(DL_TASK_ARGS);
static void world_config_exit(DL_TASK_ARGS);

static int world_config_gl_setup(void *);
static int world_config_gl_frame(void *);

static unsigned long long random_seed(void);

dltask *
world_config_appstate_alloc_detached(void)
{
	struct world_config_appstate *world_config =
		xmalloc(sizeof(*world_config));
	world_config->task = DL_TASK_INIT(world_config_entry);
	world_config->opts = (struct world_opts) {
		.seed = random_seed(),
		.size = 4096
	};
	return &world_config->task;
}

static void
world_config_entry(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct world_config_appstate, world_config, task);

	glthread_execute(world_config_gl_setup, world_config);
	snprintf(world_config->size_edit_buf, WORLDCONF_NUM_EDIT_BUFFER_LEN,
	         "%ld", world_config->opts.size);
	snprintf(world_config->seed_edit_buf, WORLDCONF_NUM_EDIT_BUFFER_LEN,
	         "%lld", world_config->opts.seed);
	world_config->next_btn_state = 0;
	world_config->exit_btn_state = 0;

	dltail(&world_config->task, world_config_loop);
}

static void
world_config_loop(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct world_config_appstate, world_config, task);

	if (glthread_execute(world_config_gl_frame, world_config) ||
	    world_config->exit_btn_state == GUI_BTN_RELEASED)
	{
		dltail(&world_config->task, world_config_exit);
		return;
	}

	if (world_config->next_btn_state == GUI_BTN_RELEASED) {
		dltask *next = viz_tectonic_appstate_alloc_detached(
		                 &world_config->opts);
		dlcontinuation(&world_config->task, world_config_exit);
		dlwait(&world_config->task, 1);
		dlnext(next, &world_config->task);
		dlasync(next);
		return;
	}

	dltail(&world_config->task, world_config_loop);
}

static void
world_config_exit(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct world_config_appstate, world_config, task);
	free(world_config);
}

static int
world_config_gl_setup(void *world_config)
{
	(void) world_config;

	glClearColor(49 / 255.0f, 59 / 255.0f, 58 / 255.0f, 1);

	return 0;
}

static int
world_config_gl_frame(void *world_config_)
{
	struct world_config_appstate *world_config = world_config_;

	window_startframe();

	unsigned font_size = 24;
	unsigned padding = 32;

	const char *title = "World Generation Configuration";
	const char *next_btn_text = "Next";
	const char *exit_btn_text = "Exit";
	const char *size_label = "World Size:";
	const char *seed_label = "Seed:";
	const char *size_err_num_label = "Must be a number";
	const char *size_err_pow_label = "Must be power of 2";
	const char *size_err_min_label = "Must be >= 1024";
	const char *seed_err_label = "World seed must be a number";
	const char *err_label = "Fix errors above to continue";

	unsigned btn_height = font_size + 16;
	unsigned btn_height_padding = btn_height + 8;

	size_t num_edit_width = WORLDCONF_NUM_EDIT_BUFFER_LEN *
	                        gui_char_width(font_size);

	struct text_opts title_opts = {
		TEXT_OPTS_DEFAULTS,
		.xoffset = padding,
		.yoffset = padding,
		.size    = font_size,
		.weight  = 255
	};

	struct text_opts size_label_opts = {
		TEXT_OPTS_DEFAULTS,
		.xoffset = padding,
		.yoffset = padding + font_size,
		.size    = font_size
	};

	struct text_opts seed_label_opts = {
		TEXT_OPTS_DEFAULTS,
		.xoffset = padding,
		.yoffset = padding + font_size * 2,
		.size    = font_size
	};

	struct edit_opts size_edit_opts = {
		EDIT_OPTS_DEFAULTS,
		.xoffset = padding + 128,
		.yoffset = padding + font_size,
		.width   = num_edit_width,
		.size    = font_size
	};

	struct edit_opts seed_edit_opts = {
		EDIT_OPTS_DEFAULTS,
		.xoffset = padding + 128,
		.yoffset = padding + font_size * 2,
		.width   = num_edit_width,
		.size    = font_size
	};

	struct text_opts size_err_opts = {
		TEXT_OPTS_DEFAULTS,
		.color   = 0xff0000ff,
		.xoffset = padding + 128 + num_edit_width + 8,
		.yoffset = padding + font_size,
		.size    = font_size
	};

	struct text_opts seed_err_opts = {
		TEXT_OPTS_DEFAULTS,
		.color   = 0xff0000ff,
		.xoffset = padding + 128 + num_edit_width + 8,
		.yoffset = padding + font_size * 2,
		.size    = font_size
	};

	struct text_opts err_opts = {
		TEXT_OPTS_DEFAULTS,
		.color   = 0xff0000ff,
		.xoffset = padding,
		.yoffset = window.height - padding -
			   btn_height_padding * 2 +
			   (btn_height - font_size) / 2,
		.size    = font_size
	};

	struct btn_opts next_btn_opts = {
		BTN_OPTS_DEFAULTS,
		.xoffset = padding,
		.yoffset = window.height - padding -
			   btn_height_padding * 2,
		.width   = window.width / 2,
		.height  = btn_height,
		.size    = font_size
	};

	struct btn_opts exit_btn_opts = {
		BTN_OPTS_DEFAULTS,
		.xoffset = padding,
		.yoffset = window.height - btn_height - padding,
		.width   = window.width / 2,
		.height  = btn_height,
		.size    = font_size
	};

	gui_text(title, strlen(title), title_opts);
	gui_text(size_label, strlen(size_label), size_label_opts);
	gui_text(seed_label, strlen(seed_label), seed_label_opts);
	gui_edit(world_config->size_edit_buf, WORLDCONF_NUM_EDIT_BUFFER_LEN, size_edit_opts);
	gui_edit(world_config->seed_edit_buf, WORLDCONF_NUM_EDIT_BUFFER_LEN, seed_edit_opts);

	unsigned long parsed_size;
	unsigned long long parsed_seed;
	{
		char *endptr;
		size_t buflen;
		errno = 0;
		buflen = strlen(world_config->size_edit_buf);
		parsed_size = strtoul(world_config->size_edit_buf, &endptr, 10);
		if ((size_t)(endptr - world_config->size_edit_buf) != buflen ||
		    buflen == 0 ||
		    errno != 0)
		{
			parsed_size = ULONG_MAX;
		}
		errno = 0;
		buflen = strlen(world_config->seed_edit_buf);
		parsed_seed = strtoull(world_config->seed_edit_buf, &endptr, 10);
		if ((size_t)(endptr - world_config->seed_edit_buf) != buflen ||
		    buflen == 0 ||
		    errno != 0)
		{
			parsed_seed = ULLONG_MAX;
		}
	}

	if (parsed_size == ULONG_MAX) {
		gui_text(size_err_num_label, strlen(size_err_num_label), size_err_opts);
	} else if (parsed_size < 1024) {
		gui_text(size_err_min_label, strlen(size_err_min_label), size_err_opts);
		parsed_size = ULONG_MAX;
	} else if (parsed_size & (parsed_size - 1)) {
		gui_text(size_err_pow_label, strlen(size_err_pow_label), size_err_opts);
		parsed_size = ULONG_MAX;
	}

	if (parsed_seed == ULLONG_MAX) {
		gui_text(seed_err_label, strlen(seed_err_label), seed_err_opts);
	}

	if (parsed_size == ULONG_MAX || parsed_seed == ULLONG_MAX) {
		gui_text_center(err_label, strlen(err_label),
		                window.width / 2, err_opts);
	} else {
		world_config->opts.size = parsed_size;
		world_config->opts.seed = parsed_seed;
		world_config->next_btn_state = gui_btn(
			world_config->next_btn_state,
			next_btn_text, strlen(next_btn_text),
			next_btn_opts);
	}

	world_config->exit_btn_state = gui_btn(
		world_config->exit_btn_state,
		exit_btn_text, strlen(exit_btn_text),
		exit_btn_opts);

	window_submitframe();

	return window.should_close;
}

/*
 * Try to read /dev/urandom, fall back to calling time.
 */
static unsigned long long
random_seed(void)
{
	FILE *dr = fopen("/dev/urandom", "r");
	if (dr) {
		unsigned long long rand;
		size_t nread = fread(&rand, sizeof(rand), 1, dr);
		if (fclose(dr))
			xperror("Error closing /dev/random FILE");
		if (nread == 1)
			return rand;
	}
	return (unsigned long long)time(NULL);
}
