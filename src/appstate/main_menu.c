#include "hammer/appstate/main_menu.h"
#include "hammer/appstate/world_config.h"
#include "hammer/cli.h"
#include "hammer/glthread.h"
#include "hammer/gui.h"
#include "hammer/mem.h"
#include "hammer/version.h"
#include "hammer/window.h"

#define VERSION_STR_MAX_LEN 128

struct main_menu_appstate {
	dltask task;
	gui_btn_state exit_btn_state;
	gui_btn_state generate_new_world_btn_state;
	char version_str[VERSION_STR_MAX_LEN];
};

static void main_menu_entry(DL_TASK_ARGS);
static void main_menu_exit (DL_TASK_ARGS);
static void main_menu_loop (DL_TASK_ARGS);

static int main_menu_gl_setup(void *);
static int main_menu_gl_frame(void *);

dltask *
main_menu_appstate_alloc_detached(void)
{
	struct main_menu_appstate *main_menu = xmalloc(sizeof(*main_menu));
	main_menu->task = DL_TASK_INIT(main_menu_entry);
	return &main_menu->task;
}

static void
main_menu_entry(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct main_menu_appstate, main_menu, task);

	glthread_execute(main_menu_gl_setup, main_menu);
	snprintf(main_menu->version_str, VERSION_STR_MAX_LEN,
	         "v%d.%d.%d %d by Rain Therrien, https://github.com/raintherrien/hammer",
	         HAMMER_VERSION_MAJOR, HAMMER_VERSION_MINOR, HAMMER_VERSION_PATCH,
	         build_date_code());
	main_menu->exit_btn_state = 0;
	main_menu->generate_new_world_btn_state = 0;

	dltail(&main_menu->task, main_menu_loop);
}

static void
main_menu_exit(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct main_menu_appstate, main_menu, task);
	free(main_menu);
	dlterminate();
}

static void
main_menu_loop(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct main_menu_appstate, main_menu, task);

	if (glthread_execute(main_menu_gl_frame, main_menu) ||
	    main_menu->exit_btn_state == GUI_BTN_RELEASED)
	{
		dltail(&main_menu->task, main_menu_exit);
		return;
	}

	if (main_menu->generate_new_world_btn_state == GUI_BTN_RELEASED) {
		dltask *next = world_config_appstate_alloc_detached();
		dlcontinuation(&main_menu->task, main_menu_entry);
		dlwait(&main_menu->task, 1);
		dlnext(next, &main_menu->task);
		dlasync(next);
		return;
	}

	dltail(&main_menu->task, main_menu_loop);
}

static int
main_menu_gl_setup(void *main_menu)
{
	(void) main_menu;

	glClearColor(49 / 255.0f, 59 / 255.0f, 58 / 255.0f, 1);

	return 0;
}

static int
main_menu_gl_frame(void *main_menu_)
{
	struct main_menu_appstate *main_menu = main_menu_;

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
	gui_text_center(main_menu->version_str, strlen(main_menu->version_str),
	                window.width, version_str_opts);

	main_menu->generate_new_world_btn_state = gui_btn(
		main_menu->generate_new_world_btn_state,
		new_world_btn_text, strlen(new_world_btn_text),
		generate_new_world_btn_opts);

	main_menu->exit_btn_state = gui_btn(
		main_menu->exit_btn_state,
		exit_btn_text, strlen(exit_btn_text),
		exit_btn_opts);

	window_submitframe();

	return 0;
}
