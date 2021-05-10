#include "hammer/appstate/world_config.h"
#include "hammer/appstate/overworld_generation.h"
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

#define NUM_EDIT_BUFFER_LEN 32

struct world_config_appstate {
	dltask task;
	struct world_opts opts;
	gui_btn_state next_btn_state;
	gui_btn_state exit_btn_state;
	int scale;
	char seed_edit_buf[NUM_EDIT_BUFFER_LEN];
};

static void world_config_entry(DL_TASK_ARGS);
static void world_config_loop(DL_TASK_ARGS);
static void world_config_exit(DL_TASK_ARGS);

static int world_config_gl_setup(void *);
static int world_config_gl_frame(void *);

static unsigned long long random_seed(void);
static unsigned long long parse_seed(struct world_config_appstate *);

dltask *
world_config_appstate_alloc_detached(void)
{
	struct world_config_appstate *world_config =
		xmalloc(sizeof(*world_config));
	world_config->task = DL_TASK_INIT(world_config_entry);
	world_config->opts = (struct world_opts) {
		.seed = random_seed(),
		.scale = 4
	};
	return &world_config->task;
}

static void
world_config_entry(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct world_config_appstate, world_config, task);

	glthread_execute(world_config_gl_setup, world_config);
	snprintf(world_config->seed_edit_buf, NUM_EDIT_BUFFER_LEN,
	         "%lld", world_config->opts.seed);
	world_config->next_btn_state = 0;
	world_config->exit_btn_state = 0;
	world_config->scale = world_config->opts.scale;

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
		dltask *next = overworld_generation_appstate_alloc_detached(
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
	unsigned border_padding = 32;
	unsigned element_padding = 8;
	float font_width = gui_char_width(font_size);

	const struct text_opts bold_text_opts = {
		TEXT_OPTS_DEFAULTS,
		.size = font_size,
		.weight = 255
	};

	const struct text_opts normal_text_opts = {
		TEXT_OPTS_DEFAULTS,
		.size = font_size
	};

	const struct edit_opts num_edit_opts = {
		EDIT_OPTS_DEFAULTS,
		.width = NUM_EDIT_BUFFER_LEN * font_width,
		.size = font_size
	};

	const struct btn_opts btn_opts = {
		BTN_OPTS_DEFAULTS,
		.width = font_width * 8,
		.height = font_size + 16,
		.size = font_size
	};

	const struct text_opts err_text_opts = {
		TEXT_OPTS_DEFAULTS,
		.color = 0xff0000ff,
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

	gui_text("World Generation Configuration", bold_text_opts);
	gui_stack_break(&stack);

	const char *scale_button_text[6] = {
		"", "Tiny", "Small", "Medium", "Large", "Compensating"
	};
	gui_text("World Scale: ", normal_text_opts);
	for (int i = 1; i <= 5; ++ i) {
		struct btn_opts scale_btn_opts = {
			BTN_OPTS_DEFAULTS,
			.xoffset = 4,
			.width = font_width * strlen(scale_button_text[i]) + 4,
			.height = font_size + 2,
			.size = font_size
		};
		if (GUI_BTN_PRESSED == gui_btn(world_config->scale == i,
		                               scale_button_text[i],
		                               scale_btn_opts))
		{
			world_config->scale = i;
		}
	}
	gui_stack_break(&stack);

	gui_text("World Seed:  ", normal_text_opts);
	gui_edit(world_config->seed_edit_buf,  NUM_EDIT_BUFFER_LEN, num_edit_opts);
	unsigned long long parsed_seed = parse_seed(world_config);
	if (parsed_seed == ULLONG_MAX) {
		gui_text("World seed must be a number", err_text_opts);
	}
	gui_stack_break(&stack);

	if (parsed_seed == ULLONG_MAX) {
		gui_text_center("Fix errors above to continue",
		                window.width / 2, err_text_opts);
	} else {
		world_config->opts.scale = world_config->scale;
		world_config->opts.seed = parsed_seed;
		world_config->next_btn_state = gui_btn(world_config->next_btn_state, "Next", btn_opts);
	}
	gui_stack_break(&stack);

	world_config->exit_btn_state = gui_btn(world_config->exit_btn_state, "Exit", btn_opts);

	gui_container_pop();

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

static unsigned long long
parse_seed(struct world_config_appstate *world_config)
{
	unsigned long long parsed_seed;
	errno = 0;
	size_t buflen = strlen(world_config->seed_edit_buf);
	char *endptr;
	parsed_seed = strtoull(world_config->seed_edit_buf, &endptr, 10);
	if ((size_t)(endptr - world_config->seed_edit_buf) != buflen ||
	    buflen == 0 || errno != 0)
	{
		parsed_seed = ULLONG_MAX;
	}
	return parsed_seed;
}
