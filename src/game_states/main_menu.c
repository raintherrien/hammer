#include "hammer/game_states/main_menu.h"
#include "hammer/glthread.h"
#include "hammer/gui.h"
#include "hammer/version.h"
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

	if (glthread_execute(main_menu_gl_frame, pkg))
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
main_menu_gl_frame(void *pkg_)
{
	struct main_menu_pkg *pkg = pkg_;

	if (window_startframe())
		return (void *)1;

	gui_text_center("Hammer", 0, 48, .weight = 255, .color = 0xc94208ff);
	gui_text_center("A collection of bad practices and anti-patterns", 48, 24);
	gui_text_center(pkg->version_str, window.height - 20, 20);

	gui_render();

	return NULL;
}
