#include "hammer/game_states/main_menu.h"
#include "hammer/game_states/worldgen_config_menu.h"
#include "hammer/glthread.h"
#include "hammer/mem.h"
#include "hammer/version.h"
#include "hammer/window.h"

static void main_menu_entry(DL_TASK_ARGS);
static void main_menu_loop(DL_TASK_ARGS);

static void *main_menu_gl_setup(void *);
static void *main_menu_gl_frame(void *);

struct main_menu_pkg
main_menu_pkg_init(struct rtargs args)
{
	return (struct main_menu_pkg) {
		.task = DL_TASK_INIT(main_menu_entry),
		.args = args
	};
}

void
main_menu_entry(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct main_menu_pkg, pkg, task);

	/* Perform set up */
	glthread_execute(main_menu_gl_setup, NULL);
	snprintf(pkg->version_str, VERSION_STR_MAX_LEN,
	         "v%d.%d.%d %d by Rain Therrien, https://github.com/raintherrien/hammer",
	         HAMMER_VERSION_MAJOR, HAMMER_VERSION_MINOR, HAMMER_VERSION_PATCH,
	         build_date_code());

	/* Kick off loop */
	dlcontinuation(&pkg->task, main_menu_loop);
	dlasync(&pkg->task);
}

static void
main_menu_loop(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct main_menu_pkg, pkg, task);

	pkg->frame_result = MAIN_MENU_LOOP;
	glthread_execute(main_menu_gl_frame, pkg);
	if (pkg->frame_result == MAIN_MENU_EXIT) {
		dlterminate();
	} else if (pkg->frame_result == MAIN_MENU_GENERATE_NEW_WORLD) {
		/* Create a worldgen config task */
		struct worldgen_config_menu_pkg *wgpkg =
			xcalloc(1, sizeof(*wgpkg));
		*wgpkg = worldgen_config_menu_init(pkg->args);
		/*
		 * Execute the worldgen config task, with this main menu
		 * linked to execute when it completes.
		 */
		dlcontinuation(&pkg->task, main_menu_entry);
		dlwait(&pkg->task, 1);
		dlnext(&wgpkg->task, &pkg->task);
		dlasync(&wgpkg->task);
	} else {
		dltail(&pkg->task);
	}
}

static void *
main_menu_gl_setup(void *_)
{
	(void) _;

	glClearColor(49 / 255.0f, 59 / 255.0f, 58 / 255.0f, 1);

	return NULL;
}

static void *
main_menu_gl_frame(void *pkg_)
{
	struct main_menu_pkg *pkg = pkg_;

	if (window_startframe()) {
		pkg->frame_result = MAIN_MENU_EXIT;
		return NULL;
	}

	const char *title = "Hammer";
	const char *subtitle = "A collection of bad practices and anti-patterns";

	gui_text_center(title, strlen(title), window.width, (struct text_opts) {
		TEXT_OPTS_DEFAULTS,
		.xoffset = 0,
		.yoffset = 0,
		.size    = 48,
		.weight  = 255,
		.color   = 0xc94208ff
	});

	gui_text_center(subtitle, strlen(subtitle), window.width, (struct text_opts) {
		TEXT_OPTS_DEFAULTS,
		.xoffset = 0,
		.yoffset = 48,
		.size    = 24
	});

	gui_text_center(pkg->version_str, strlen(pkg->version_str), window.width, (struct text_opts) {
		TEXT_OPTS_DEFAULTS,
		.xoffset = 0,
		.yoffset = window.height - 20,
		.size    = 20
	});

	unsigned button_height = 48;
	unsigned button_font_size = button_height - 16;

	const char *new_world_btn_text = "Generate new world!";
	const char *exit_btn_text = "Exit";

	pkg->generate_new_world_button_state = gui_button(
		pkg->generate_new_world_button_state,
		new_world_btn_text, strlen(new_world_btn_text),
		(struct button_opts) {
			BUTTON_OPTS_DEFAULTS,
			.xoffset = window.width / 4,
			.yoffset = window.height / 2 - button_height / 2,
			.width   = window.width / 2,
			.height  = button_height,
			.size    = button_font_size
		});

	pkg->exit_button_state = gui_button(
		pkg->exit_button_state,
		exit_btn_text, strlen(exit_btn_text),
		(struct button_opts) {
			BUTTON_OPTS_DEFAULTS,
			.xoffset = window.width / 4,
			.yoffset = window.height - 48 - button_height,
			.width   = window.width / 2,
			.height  = button_height,
			.size    = button_font_size
		});

	if (pkg->generate_new_world_button_state == GUI_BUTTON_RELEASED)
		pkg->frame_result = MAIN_MENU_GENERATE_NEW_WORLD;

	if (pkg->exit_button_state == GUI_BUTTON_RELEASED)
		pkg->frame_result = MAIN_MENU_EXIT;

	window_submitframe();

	return NULL;
}
