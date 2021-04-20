#include "hammer/game_states/worldgen_config_menu.h"
#include "hammer/game_states/basic_worldgen.h"
#include "hammer/glthread.h"
#include "hammer/mem.h"
#include "hammer/version.h"
#include "hammer/window.h"
#include <errno.h>
#include <limits.h>

static void worldgen_config_entry(DL_TASK_ARGS);
static void worldgen_config_loop(DL_TASK_ARGS);

static void *worldgen_config_gl_setup(void *);
static void *worldgen_config_gl_frame(void *);

struct worldgen_config_menu_pkg
worldgen_config_menu_init(struct rtargs args)
{
	return (struct worldgen_config_menu_pkg) {
		.task = DL_TASK_INIT(worldgen_config_entry),
		.args = args
	};
}

static void
worldgen_config_entry(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct worldgen_config_menu_pkg, pkg, task);

	/* Perform set up */
	glthread_execute(worldgen_config_gl_setup, NULL);
	snprintf(pkg->size_edit_buffer, WORLDGEN_SIZE_SEED_EDIT_BUFFER_LEN,
	         "%ld", pkg->args.size);
	snprintf(pkg->seed_edit_buffer, WORLDGEN_SIZE_SEED_EDIT_BUFFER_LEN,
	         "%lld", pkg->args.seed);

	/* Kick off loop */
	dlcontinuation(&pkg->task, worldgen_config_loop);
	dlasync(&pkg->task);
}

static void
worldgen_config_loop(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct worldgen_config_menu_pkg, pkg, task);

	pkg->frame_result = WORLDGEN_LOOP;
	glthread_execute(worldgen_config_gl_frame, pkg);
	if (pkg->frame_result == WORLDGEN_LOOP) {
		dltail(&pkg->task);
	} else if (pkg->frame_result == WORLDGEN_NEXT) {
		/* Create a worldgen task */
		struct basic_worldgen_pkg *wgpkg = xcalloc(1, sizeof(*wgpkg));
		*wgpkg = basic_worldgen_init(pkg->args);
		dlswap(&pkg->task, &wgpkg->task);
		free(pkg);
	}
}

static void *
worldgen_config_gl_setup(void *_)
{
	(void) _;

	glClearColor(49 / 255.0f, 59 / 255.0f, 58 / 255.0f, 1);

	return NULL;
}

static void *
worldgen_config_gl_frame(void *pkg_)
{
	struct worldgen_config_menu_pkg *pkg = pkg_;

	if (window_startframe()) {
		pkg->frame_result = WORLDGEN_EXIT;
		return NULL;
	}

	unsigned font_size = 24;
	unsigned padding = 32;

	const char *title = "World Generation Configuration";
	const char *next_btn_text = "Next";
	const char *exit_btn_text = "Exit";
	const char *world_size_label = "World Size:";
	const char *seed_label = "Seed:";
	const char *size_err_label = "Invalid world size";
	const char *seed_err_label = "Invalid world seed";
	const char *err_label = "Fix errors above to continue";

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

	size_t size_seed_edit_width = WORLDGEN_SIZE_SEED_EDIT_BUFFER_LEN *
	                              gui_char_width(font_size);

	gui_edit(pkg->size_edit_buffer, WORLDGEN_SIZE_SEED_EDIT_BUFFER_LEN, (struct edit_opts) {
		EDIT_OPTS_DEFAULTS,
		.xoffset = padding + 128,
		.yoffset = padding + font_size,
		.width   = size_seed_edit_width,
		.size    = font_size
	});

	gui_edit(pkg->seed_edit_buffer, WORLDGEN_SIZE_SEED_EDIT_BUFFER_LEN, (struct edit_opts) {
		EDIT_OPTS_DEFAULTS,
		.xoffset = padding + 128,
		.yoffset = padding + font_size * 2,
		.width   = size_seed_edit_width,
		.size    = font_size
	});

	unsigned long parsed_size;
	unsigned long long parsed_seed;
	{
		char *endptr;
		size_t buflen;
		errno = 0;
		buflen = strlen(pkg->size_edit_buffer);
		parsed_size = strtoul(pkg->size_edit_buffer, &endptr, 10);
		if ((size_t)(endptr - pkg->size_edit_buffer) != buflen ||
		    errno != 0)
		{
			parsed_size = ULONG_MAX;
		}
		errno = 0;
		buflen = strlen(pkg->seed_edit_buffer);
		parsed_seed = strtoull(pkg->seed_edit_buffer, &endptr, 10);
		if ((size_t)(endptr - pkg->seed_edit_buffer) != buflen ||
		    errno != 0)
		{
			parsed_seed = ULLONG_MAX;
		}
	}

	if (parsed_size == ULONG_MAX) {
		gui_text(size_err_label, strlen(size_err_label), (struct text_opts) {
			TEXT_OPTS_DEFAULTS,
			.color   = 0xff0000ff,
			.xoffset = padding + 128 + size_seed_edit_width + 8,
			.yoffset = padding + font_size,
			.size    = font_size
		});
	}

	if (parsed_seed == ULLONG_MAX) {
		gui_text(seed_err_label, strlen(seed_err_label), (struct text_opts) {
			TEXT_OPTS_DEFAULTS,
			.color   = 0xff0000ff,
			.xoffset = padding + 128 + size_seed_edit_width + 8,
			.yoffset = padding + font_size * 2,
			.size    = font_size
		});
	}

	unsigned button_height = font_size + 16;
	unsigned button_height_padding = button_height + 8;

	if (parsed_size == ULONG_MAX || parsed_seed == ULLONG_MAX) {
		gui_text_center(err_label, strlen(err_label), window.width / 2, (struct text_opts) {
			TEXT_OPTS_DEFAULTS,
			.color   = 0xff0000ff,
			.xoffset = padding,
			.yoffset = window.height - padding -
			           button_height_padding * 2 +
			           (button_height - font_size) / 2,
			.size    = font_size
		});
	} else {
		pkg->args.size = parsed_size;
		pkg->args.seed = parsed_seed;
		pkg->next_button_state = gui_button(
			pkg->next_button_state,
			next_btn_text, strlen(next_btn_text),
			(struct button_opts) {
				BUTTON_OPTS_DEFAULTS,
				.xoffset = padding,
				.yoffset = window.height - padding -
				           button_height_padding * 2,
				.width   = window.width / 2,
				.height  = button_height,
				.size    = font_size
			});

		if (pkg->next_button_state == GUI_BUTTON_RELEASED)
			pkg->frame_result = WORLDGEN_NEXT;
	}

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
