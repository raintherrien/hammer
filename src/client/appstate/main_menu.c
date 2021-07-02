#include "hammer/client/appstate.h"
#include "hammer/client/glthread.h"
#include "hammer/client/gui.h"
#include "hammer/client/window.h"
#include "hammer/version.h"

#define VERSION_STR_MAX_LEN 128

dltask appstate_main_menu_frame;

static struct {
	gui_btn_state exit_btn_state;
	gui_btn_state generate_new_world_btn_state;
	char version_str[VERSION_STR_MAX_LEN];
} main_menu;

static void main_menu_frame_async(DL_TASK_ARGS);
static int  main_menu_gl_setup(void *);
static int  main_menu_gl_frame(void *);

void
appstate_main_menu_setup(void)
{
	appstate_main_menu_frame = DL_TASK_INIT(main_menu_frame_async);

	glthread_execute(main_menu_gl_setup, NULL);
	snprintf(main_menu.version_str, VERSION_STR_MAX_LEN,
	         "v%d.%d.%d %d by Rain Therrien, https://github.com/raintherrien/hammer",
	         HAMMER_VERSION_MAJOR, HAMMER_VERSION_MINOR, HAMMER_VERSION_PATCH,
	         build_date_code());
	main_menu.exit_btn_state = 0;
	main_menu.generate_new_world_btn_state = 0;
}

void
appstate_main_menu_teardown(void)
{
	/* Nothing to free */
}

static void
main_menu_frame_async(DL_TASK_ARGS)
{
	DL_TASK_ENTRY_VOID;

	if (glthread_execute(main_menu_gl_frame, NULL) ||
	    main_menu.exit_btn_state == GUI_BTN_RELEASED)
	{
		appstate_transition(APPSTATE_TRANSITION_MAIN_MENU_CLOSE);
		return;
	}

	if (main_menu.generate_new_world_btn_state == GUI_BTN_RELEASED) {
		appstate_transition(APPSTATE_TRANSITION_SERVER_CONFIG);
		return;
	}
}

static int
main_menu_gl_setup(void *_)
{
	glClearColor(49 / 255.0f, 59 / 255.0f, 58 / 255.0f, 1);

	return 0;
}

static int
main_menu_gl_frame(void *_)
{
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

	gui_text_center("Hammer", window.width, title_opts);
	gui_text_center("A collection of bad practices and anti-patterns", window.width, subtitle_opts);
	gui_text_center(main_menu.version_str, window.width, version_str_opts);
	main_menu.generate_new_world_btn_state = gui_btn(main_menu.generate_new_world_btn_state, "Generate new world!", generate_new_world_btn_opts);
	main_menu.exit_btn_state = gui_btn(main_menu.exit_btn_state, "Exit", exit_btn_opts);

	window_submitframe();

	return window.should_close;
}
