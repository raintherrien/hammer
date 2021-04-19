#include "hammer/game_states/worldgen_menu.h"
#include "hammer/glthread.h"
#include "hammer/mem.h"
#include "hammer/version.h"
#include "hammer/window.h"

static void worldgen_entry(DL_TASK_ARGS);
static void worldgen_loop(DL_TASK_ARGS);

static void *worldgen_gl_setup(void *);
static void *worldgen_gl_frame(void *);

static char xxx_size_buffer[32] = "\0";
static char xxx_seed_buffer[32] = "\0";

struct worldgen_menu_pkg
worldgen_menu_pkg_init(struct rtargs args)
{
	return (struct worldgen_menu_pkg) {
		.task = DL_TASK_INIT(worldgen_entry),
		.args = args
	};
}

static void
worldgen_entry(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct worldgen_menu_pkg, pkg, task);

	/* Perform set up */
	glthread_execute(worldgen_gl_setup, NULL);
	snprintf(xxx_size_buffer, 32, "%ld", pkg->args.size);
	snprintf(xxx_seed_buffer, 32, "%lld", pkg->args.seed);

	/* Kick off loop */
	dlcontinuation(&pkg->task, worldgen_loop);
	dlasync(&pkg->task);
}

static void
worldgen_loop(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct worldgen_menu_pkg, pkg, task);

	pkg->frame_result = WORLDGEN_LOOP;
	glthread_execute(worldgen_gl_frame, pkg);
	if (pkg->frame_result == WORLDGEN_LOOP)
		dltail(&pkg->task);
}

static void *
worldgen_gl_setup(void *_)
{
	(void) _;

	glClearColor(49 / 255.0f, 59 / 255.0f, 58 / 255.0f, 1);

	return NULL;
}

static void *
worldgen_gl_frame(void *pkg_)
{
	struct worldgen_menu_pkg *pkg = pkg_;

	if (window_startframe()) {
		pkg->frame_result = WORLDGEN_EXIT;
		return NULL;
	}

	unsigned font_size = 24;
	unsigned padding = 32;

	const char *title = "World Generation Configuration";
	const char *exit_btn_text = "Exit";
	const char *world_size_label = "World Size:";
	const char *seed_label = "Seed:";

	gui_text(title, strlen(title), (struct text_opts) {
		TEXT_OPTS_DEFAULTS,
		.xoffset = padding,
		.yoffset = padding,
		.size    = font_size,
		.weight  = 255
	});
	gui_text(world_size_label, strlen(world_size_label), (struct text_opts) {
		TEXT_OPTS_DEFAULTS,
		.xoffset = padding,
		.yoffset = padding + font_size,
		.size    = font_size
	});
	gui_text(seed_label, strlen(seed_label), (struct text_opts) {
		TEXT_OPTS_DEFAULTS,
		.xoffset = padding,
		.yoffset = padding + font_size * 2,
		.size    = font_size
	});

	gui_edit(xxx_size_buffer, sizeof(xxx_size_buffer), (struct edit_opts) {
		EDIT_OPTS_DEFAULTS,
		.xoffset = padding + 128,
		.yoffset = padding + font_size,
		.width   = 31 * gui_char_width(font_size),
		.size    = font_size
	});

	gui_edit(xxx_seed_buffer, sizeof(xxx_seed_buffer), (struct edit_opts) {
		EDIT_OPTS_DEFAULTS,
		.xoffset = padding + 128,
		.yoffset = padding + font_size * 2,
		.width   = 31 * gui_char_width(font_size),
		.size    = font_size
	});

	unsigned button_height = font_size + 16;
	pkg->exit_button_state = gui_button(
		pkg->exit_button_state,
		exit_btn_text, strlen(exit_btn_text),
		(struct button_opts) {
			BUTTON_OPTS_DEFAULTS,
			.xoffset = padding,
			.yoffset = window.height - button_height - padding,
			.width   = window.width / 2,
			.height  = button_height,
			.size    = font_size
		});

	if (pkg->exit_button_state == GUI_BUTTON_RELEASED)
		pkg->frame_result = WORLDGEN_EXIT;

	window_submitframe();

	return NULL;
}
