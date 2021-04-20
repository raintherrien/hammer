#include "hammer/game_states/basic_worldgen.h"
#include "hammer/glthread.h"
#include "hammer/window.h"

static void basic_worldgen_entry(DL_TASK_ARGS);
static void basic_worldgen_loop(DL_TASK_ARGS);

static void *basic_worldgen_gl_setup(void *);
static void *basic_worldgen_gl_frame(void *);

struct basic_worldgen_pkg
basic_worldgen_init(struct rtargs args)
{
	return (struct basic_worldgen_pkg) {
		.task = DL_TASK_INIT(basic_worldgen_entry),
		.args = args
	};
}


static void
basic_worldgen_entry(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct basic_worldgen_pkg, pkg, task);

	/* Perform set up */
	glthread_execute(basic_worldgen_gl_setup, NULL);

	/* Kick off loop */
	dlcontinuation(&pkg->task, basic_worldgen_loop);
	dlasync(&pkg->task);
}

static void
basic_worldgen_loop(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct basic_worldgen_pkg, pkg, task);

	pkg->frame_result = BASIC_WORLDGEN_LOOP;
	glthread_execute(basic_worldgen_gl_frame, pkg);
	switch (pkg->frame_result) {
	case BASIC_WORLDGEN_LOOP:
		dltail(&pkg->task);
		break;
	case BASIC_WORLDGEN_EXIT:
		free(pkg);
		/* Return to sender */
		break;
	}
}

static void *
basic_worldgen_gl_setup(void *_)
{
	(void) _;

	glClearColor(49 / 255.0f, 59 / 255.0f, 58 / 255.0f, 1);

	return NULL;
}

static void *
basic_worldgen_gl_frame(void *pkg_)
{
	struct basic_worldgen_pkg *pkg = pkg_;

	if (window_startframe()) {
		pkg->frame_result = BASIC_WORLDGEN_EXIT;
		return NULL;
	}

	const char *exit_text = "Exit";
	const char *todotxt = "TODO: Render 2D terrain generation";

	unsigned font_size = 24;
	unsigned padding = 32;
	unsigned button_height = font_size + 16;

	gui_text(todotxt, strlen(todotxt), (struct text_opts) {
		TEXT_OPTS_DEFAULTS,
		.size = font_size,
		.xoffset = window.width / 2 - strlen(todotxt) / 2 *
		                                gui_char_width(font_size),
		.yoffset = window.height / 2 - font_size / 2,
	});

	pkg->exit_button_state = gui_button(
		pkg->exit_button_state,
		exit_text, strlen(exit_text),
		(struct button_opts) {
			BUTTON_OPTS_DEFAULTS,
			.xoffset = window.width - window.width / 2 - padding,
			.yoffset = window.height - button_height - padding,
			.width   = window.width / 2,
			.height  = button_height,
			.size    = font_size
		});

	if (pkg->exit_button_state == GUI_BUTTON_RELEASED)
		pkg->frame_result = BASIC_WORLDGEN_EXIT;

	window_submitframe();

	return NULL;
}
