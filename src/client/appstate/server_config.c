#include "hammer/appstate/server_config.h"
#include "hammer/appstate.h"
#include "hammer/error.h"
#include "hammer/glthread.h"
#include "hammer/gui.h"
#include "hammer/server.h"
#include "hammer/window.h"
#include <limits.h>
#include <stdio.h>
#include <time.h>

#define NUM_EDIT_BUFFER_LEN 32

dltask appstate_server_config_frame;

static struct {
	gui_btn_state next_btn_state;
	gui_btn_state exit_btn_state;
	int scale;
	char seed_edit_buf[NUM_EDIT_BUFFER_LEN];
} server_config;

static void server_config_frame_async(DL_TASK_ARGS);

static int server_config_gl_setup(void *);
static int server_config_gl_frame(void *);

static unsigned long long random_seed(void);
static unsigned long long parse_seed(void);

void
appstate_server_config_setup(void)
{
	appstate_server_config_frame = DL_TASK_INIT(server_config_frame_async);
	server.world.opts = (struct world_opts) {
		.seed = random_seed(),
		.scale = 3,
		.tectonic = {
			.collision_xfer   = 0.035f,
			.subduction_xfer  = 0.025f,
			.merge_ratio      = 0.2f,
			.rift_mass        = 0.9f,
			.volcano_mass     = 15.0f,
			.volcano_chance   = 0.01f,
			.continent_talus  = 0.05f,
			.ocean_talus      = 0.025f,
			.generation_steps = 100,
			.generations      = 2,
			.min_plates       = 10,
			.max_plates       = 25,
			.segment_radius   = 2,
			.divergent_radius = 5,
			.erosion_ticks    = 5,
			.rift_ticks       = 60
		}
	};
	glthread_execute(server_config_gl_setup, NULL);
	snprintf(server_config.seed_edit_buf, NUM_EDIT_BUFFER_LEN,
	         "%llu", server.world.opts.seed);
	server_config.next_btn_state = 0;
	server_config.exit_btn_state = 0;
	server_config.scale = server.world.opts.scale;
}

void
appstate_server_config_teardown(void)
{
	/* Nothing to free */
}

static void
server_config_frame_async(DL_TASK_ARGS)
{
	DL_TASK_ENTRY_VOID;

	if (glthread_execute(server_config_gl_frame, NULL) ||
	    server_config.exit_btn_state == GUI_BTN_RELEASED)
	{
		appstate_transition(APPSTATE_TRANSITION_SERVER_CONFIG_CANCEL);
		return;
	}

	if (server_config.next_btn_state == GUI_BTN_RELEASED) {
		appstate_transition(APPSTATE_TRANSITION_SERVER_PLANET_GEN);
		return;
	}
}

static int
server_config_gl_setup(void *_)
{
	glClearColor(49 / 255.0f, 59 / 255.0f, 58 / 255.0f, 1);

	return 0;
}

static int
server_config_gl_frame(void *_)
{
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
		if (GUI_BTN_PRESSED == gui_btn(server_config.scale == i,
		                               scale_button_text[i],
		                               scale_btn_opts))
		{
			server_config.scale = i;
		}
	}
	gui_stack_break(&stack);

	gui_text("World Seed:  ", normal_text_opts);
	gui_edit(server_config.seed_edit_buf,  NUM_EDIT_BUFFER_LEN, num_edit_opts);
	unsigned long long parsed_seed = parse_seed();
	if (parsed_seed == ULLONG_MAX) {
		gui_text("World seed must be a number", err_text_opts);
	}
	gui_stack_break(&stack);

	if (parsed_seed == ULLONG_MAX) {
		gui_text_center("Fix errors above to continue",
		                window.width / 2, err_text_opts);
	} else {
		server.world.opts.scale = server_config.scale;
		server.world.opts.seed = parsed_seed;
		server_config.next_btn_state = gui_btn(server_config.next_btn_state, "Next", btn_opts);
	}
	gui_stack_break(&stack);

	server_config.exit_btn_state = gui_btn(server_config.exit_btn_state, "Exit", btn_opts);

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
parse_seed(void)
{
	unsigned long long parsed_seed;
	errno = 0;
	size_t buflen = strlen(server_config.seed_edit_buf);
	char *endptr;
	parsed_seed = strtoull(server_config.seed_edit_buf, &endptr, 10);
	if ((size_t)(endptr - server_config.seed_edit_buf) != buflen ||
	    buflen == 0 || errno != 0)
	{
		parsed_seed = ULLONG_MAX;
	}
	return parsed_seed;
}
