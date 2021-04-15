#include "hammer/game_states/main_menu.h"
#include "hammer/glthread.h"
#include "hammer/gui.h"
#include "hammer/window.h"

static void main_menu_loop(DL_TASK_ARGS);

static void *main_menu_gl_setup(void *);
static void *main_menu_gl_frame(void *);

void
main_menu_entry(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct main_menu_pkg, pkg, task);

	/* Perform set up */
	glthread_execute(main_menu_gl_setup, NULL);

	/* Kick off loop */
	dlcontinuation(&pkg->task, main_menu_loop);
	dlasync(&pkg->task);
}

static void
main_menu_loop(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct main_menu_pkg, pkg, task);

	if (glthread_execute(main_menu_gl_frame, NULL))
		dlterminate();
	else
		dltail(&pkg->task);
}

static void *
main_menu_gl_setup(void *_)
{
	(void) _;

	glClearColor(49 / 255.0f, 59 / 255.0f, 58 / 255.0f, 1);

	return NULL;
}

static void *
main_menu_gl_frame(void *_)
{
	(void) _;

	if (window_startframe())
		return (void *)1;

	float title_width  = window.width / 2;
	float title_height = title_width / gui_text_ratio;
	float title_left   = window.width / 4;
	float title_top    = window.height / 4;
	gui_text("Hammer", title_left, title_top, title_width, title_height);

	gui_render();

	return NULL;
}
