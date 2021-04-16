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
	         "Hammer v%d.%d.%d %d",
	         HAMMER_VERSION_MAJOR, HAMMER_VERSION_MINOR, HAMMER_VERSION_PATCH,
	         build_date_code());
	pkg->version_str_len = strlen(pkg->version_str);
	memset(pkg->bork_str, 0, sizeof(*pkg->bork_str)*VERSION_STR_MAX_LEN);

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

	char *c = window.text_input;
	if (*c && pkg->bork_str_len < VERSION_STR_MAX_LEN-1) {
		if (*c != '\b')
			pkg->bork_str[pkg->bork_str_len ++] = *c;
		else if (pkg->bork_str_len > 0)
			pkg->bork_str[-- pkg->bork_str_len] = '\0';
		++ c;
	}

	float font_size = 32;
	gui_text(pkg->version_str, 0, 0, font_size, 1,1,1);
	gui_text("What an absolute shit show", 0, font_size, 16, 1,0,0);
	gui_text(pkg->bork_str, 0, font_size+16, 24, 0,1,0);

	gui_render();

	return NULL;
}
