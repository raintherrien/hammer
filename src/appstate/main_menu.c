#include "hammer/appstate/main_menu.h"
#include "hammer/appstate/manager.h"
#include "hammer/appstate/world_config.h"
#include "hammer/glthread.h"
#include "hammer/mem.h"
#include "hammer/version.h"
#include "hammer/window.h"

static void main_menu_entry(void *);
static void main_menu_exit(void *);
static void main_menu_loop(void *, struct appstate_cmd *);

static int main_menu_gl_setup(void *);
static int main_menu_gl_frame(void *);

struct appstate
appstate_main_menu_alloc(struct rtargs args)
{
	struct appstate_main_menu *state = xmalloc(sizeof(*state));

	*state = (struct appstate_main_menu) {
		.args = args
	};

	return (struct appstate) {
		.entry_fn = main_menu_entry,
		.exit_fn  = main_menu_exit,
		.loop_fn  = main_menu_loop,
		.arg      = state
	};
}

static void
main_menu_entry(void *state_)
{
	struct appstate_main_menu *state = state_;

	glthread_execute(main_menu_gl_setup, NULL);
	snprintf(state->version_str, VERSION_STR_MAX_LEN,
	         "v%d.%d.%d %d by Rain Therrien, https://github.com/raintherrien/hammer",
	         HAMMER_VERSION_MAJOR, HAMMER_VERSION_MINOR, HAMMER_VERSION_PATCH,
	         build_date_code());
	state->exit_btn_state = 0;
	state->generate_new_world_btn_state = 0;
}

static void
main_menu_exit(void *_)
{
	(void)_;
}

static void
main_menu_loop(void *state_, struct appstate_cmd *cmd)
{
	struct appstate_main_menu *state = state_;

	if (glthread_execute(main_menu_gl_frame, state))
		cmd->type = APPSTATE_CMD_POP;

	if (state->exit_btn_state == GUI_BTN_RELEASED)
		cmd->type = APPSTATE_CMD_POP;

	if (state->generate_new_world_btn_state == GUI_BTN_RELEASED) {
		cmd->type = APPSTATE_CMD_PUSH;
		cmd->newstate = appstate_world_config_alloc(state->args);
	}
}

static int
main_menu_gl_setup(void *_)
{
	(void) _;

	glClearColor(49 / 255.0f, 59 / 255.0f, 58 / 255.0f, 1);

	return 0;
}

static int
main_menu_gl_frame(void *state_)
{
	struct appstate_main_menu *state = state_;

	if (window_startframe())
		return 1;

	const char *title = "Hammer";
	const char *subtitle = "A collection of bad practices and anti-patterns";
	const char *new_world_btn_text = "Generate new world!";
	const char *exit_btn_text = "Exit";

	unsigned btn_height = 48;
	unsigned btn_font_size = btn_height - 16;

	struct text_opts title_opts = {
		TEXT_OPTS_DEFAULTS,
		.xoffset = 0,
		.yoffset = 0,
		.size    = 48,
		.weight  = 255,
		.color   = 0xc94208ff
	};

	struct text_opts subtitle_opts = {
		TEXT_OPTS_DEFAULTS,
		.xoffset = 0,
		.yoffset = 48,
		.size    = 24
	};

	struct text_opts version_str_opts = {
		TEXT_OPTS_DEFAULTS,
		.xoffset = 0,
		.yoffset = window.height - 20,
		.size    = 20
	};

	struct btn_opts generate_new_world_btn_opts = {
		BTN_OPTS_DEFAULTS,
		.xoffset = window.width / 4,
		.yoffset = window.height / 2 - btn_height / 2,
		.width   = window.width / 2,
		.height  = btn_height,
		.size    = btn_font_size
	};

	struct btn_opts exit_btn_opts = {
		BTN_OPTS_DEFAULTS,
		.xoffset = window.width / 4,
		.yoffset = window.height - 48 - btn_height,
		.width   = window.width / 2,
		.height  = btn_height,
		.size    = btn_font_size
	};

	gui_text_center(title, strlen(title), window.width, title_opts);
	gui_text_center(subtitle, strlen(subtitle), window.width, subtitle_opts);
	gui_text_center(state->version_str, strlen(state->version_str),
	                window.width, version_str_opts);

	state->generate_new_world_btn_state = gui_btn(
		state->generate_new_world_btn_state,
		new_world_btn_text, strlen(new_world_btn_text),
		generate_new_world_btn_opts);

	state->exit_btn_state = gui_btn(
		state->exit_btn_state,
		exit_btn_text, strlen(exit_btn_text),
		exit_btn_opts);

	window_submitframe();

	return 0;
}
