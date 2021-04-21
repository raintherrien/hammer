#include "hammer/appstate/world_config.h"
#include "hammer/appstate/manager.h"
#include "hammer/appstate/visualize_tectonic.h"
#include "hammer/glthread.h"
#include "hammer/mem.h"
#include "hammer/version.h"
#include "hammer/window.h"
#include <errno.h>
#include <limits.h>

static void world_config_entry(void *);
static void world_config_exit(void *);
static void world_config_loop(void *, struct appstate_cmd *);

static int world_config_gl_setup(void *);
static int world_config_gl_frame(void *);

struct appstate
appstate_world_config_alloc(struct rtargs args)
{
	struct appstate_world_config *state = xmalloc(sizeof(*state));

	*state = (struct appstate_world_config) {
		.args = args
	};

	return (struct appstate) {
		.entry_fn = world_config_entry,
		.exit_fn  = world_config_exit,
		.loop_fn  = world_config_loop,
		.arg      = state
	};
}

static void
world_config_entry(void *state_)
{
	struct appstate_world_config *state = state_;

	glthread_execute(world_config_gl_setup, NULL);
	snprintf(state->size_edit_buf, WORLDCONF_NUM_EDIT_BUFFER_LEN,
	         "%ld", state->args.size);
	snprintf(state->seed_edit_buf, WORLDCONF_NUM_EDIT_BUFFER_LEN,
	         "%lld", state->args.seed);
	state->next_btn_state = 0;
	state->exit_btn_state = 0;
}

static void
world_config_exit(void *_)
{
	(void)_;
}

static void
world_config_loop(void *state_, struct appstate_cmd *cmd)
{
	struct appstate_world_config *state = state_;

	if (glthread_execute(world_config_gl_frame, state))
		cmd->type = APPSTATE_CMD_POP;

	if (state->exit_btn_state == GUI_BTN_RELEASED)
		cmd->type = APPSTATE_CMD_POP;

	if (state->next_btn_state == GUI_BTN_RELEASED) {
		cmd->type = APPSTATE_CMD_SWAP;
		cmd->newstate = appstate_visualize_tectonic_alloc(state->args);
	}
}

static int
world_config_gl_setup(void *_)
{
	(void) _;

	glClearColor(49 / 255.0f, 59 / 255.0f, 58 / 255.0f, 1);

	return 0;
}

static int
world_config_gl_frame(void *state_)
{
	struct appstate_world_config *state = state_;

	if (window_startframe())
		return 1;

	unsigned font_size = 24;
	unsigned padding = 32;

	const char *title = "World Generation Configuration";
	const char *next_btn_text = "Next";
	const char *exit_btn_text = "Exit";
	const char *size_label = "World Size:";
	const char *seed_label = "Seed:";
	const char *size_err_label = "Invalid world size";
	const char *seed_err_label = "Invalid world seed";
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
	gui_edit(state->size_edit_buf, WORLDCONF_NUM_EDIT_BUFFER_LEN, size_edit_opts);
	gui_edit(state->seed_edit_buf, WORLDCONF_NUM_EDIT_BUFFER_LEN, seed_edit_opts);

	unsigned long parsed_size;
	unsigned long long parsed_seed;
	{
		char *endptr;
		size_t buflen;
		errno = 0;
		buflen = strlen(state->size_edit_buf);
		parsed_size = strtoul(state->size_edit_buf, &endptr, 10);
		if ((size_t)(endptr - state->size_edit_buf) != buflen ||
		    buflen == 0 ||
		    errno != 0)
		{
			parsed_size = ULONG_MAX;
		}
		errno = 0;
		buflen = strlen(state->seed_edit_buf);
		parsed_seed = strtoull(state->seed_edit_buf, &endptr, 10);
		if ((size_t)(endptr - state->seed_edit_buf) != buflen ||
		    buflen == 0 ||
		    errno != 0)
		{
			parsed_seed = ULLONG_MAX;
		}
	}

	if (parsed_size == ULONG_MAX) {
		gui_text(size_err_label, strlen(size_err_label), size_err_opts);
	}

	if (parsed_seed == ULLONG_MAX) {
		gui_text(seed_err_label, strlen(seed_err_label), seed_err_opts);
	}

	if (parsed_size == ULONG_MAX || parsed_seed == ULLONG_MAX) {
		gui_text_center(err_label, strlen(err_label),
		                window.width / 2, err_opts);
	} else {
		state->args.size = parsed_size;
		state->args.seed = parsed_seed;
		state->next_btn_state = gui_btn(
			state->next_btn_state,
			next_btn_text, strlen(next_btn_text),
			next_btn_opts);
	}

	state->exit_btn_state = gui_btn(
		state->exit_btn_state,
		exit_btn_text, strlen(exit_btn_text),
		exit_btn_opts);

	window_submitframe();

	return 0;
}
