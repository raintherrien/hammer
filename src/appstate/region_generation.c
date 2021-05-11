#include "hammer/appstate/region_generation.h"
#include "hammer/glthread.h"
#include "hammer/gui.h"
#include "hammer/mem.h"
#include "hammer/window.h"

struct region_generation_appstate {
	dltask                     task;
	const struct climate      *climate;
	const struct lithosphere  *lithosphere;
	const struct stream_graph *stream_graph;
	const struct world_opts   *world_opts;
	int                        cancel_btn_state;
};

static void region_generation_entry(DL_TASK_ARGS);
static void region_generation_exit(DL_TASK_ARGS);

static int region_generation_gl_setup(void *);
static int region_generation_gl_frame(void *);

static void viz_region_loop(DL_TASK_ARGS);

dltask *
region_generation_appstate_alloc_detached(
	const struct climate *climate,
	const struct lithosphere *lithosphere,
	const struct stream_graph *stream_graph,
	const struct world_opts *world_opts)
{
	struct region_generation_appstate *rg = xmalloc(sizeof(*rg));
	rg->task         = DL_TASK_INIT(region_generation_entry);
	rg->climate      = climate;
	rg->lithosphere  = lithosphere;
	rg->stream_graph = stream_graph;
	rg->world_opts   = world_opts;
	return &rg->task;
}

static void
region_generation_entry(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct region_generation_appstate, rg, task);
	rg->cancel_btn_state = 0;
	glthread_execute(region_generation_gl_setup, rg);
	dltail(&rg->task, viz_region_loop);
}

static void
region_generation_exit(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct region_generation_appstate, rg, task);
	free(rg);
}

static void
viz_region_loop(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct region_generation_appstate, rg, task);

	if (glthread_execute(region_generation_gl_frame, rg) ||
	    rg->cancel_btn_state == GUI_BTN_RELEASED)
	{
		dltail(&rg->task, region_generation_exit);
		return;
	}

	dltail(&rg->task, viz_region_loop);
}

static int
region_generation_gl_setup(void *rg_)
{
	(void)rg_;
	glClearColor(49 / 255.0f, 59 / 255.0f, 58 / 255.0f, 1);
	return 0;
}

static int
region_generation_gl_frame(void *rg_)
{
	struct region_generation_appstate *rg = rg_;

	unsigned font_size = 24;
	unsigned border_padding = 32;
	unsigned element_padding = 8;

	const struct text_opts bold_text_opts = {
		TEXT_OPTS_DEFAULTS,
		.size = font_size,
		.weight = 255
	};

	const struct text_opts normal_text_opts = {
		TEXT_OPTS_DEFAULTS,
		.size = font_size
	};

	const struct text_opts small_text_opts = {
		TEXT_OPTS_DEFAULTS,
		.size = 20
	};

	struct btn_opts btn_opts = {
		BTN_OPTS_DEFAULTS,
		.width = font_size * 8,
		.height = font_size + 16,
		.size = font_size
	};

	struct check_opts check_opts = {
		BTN_OPTS_DEFAULTS,
		.size = font_size,
		.width = font_size,
		.height = font_size,
	};

	gui_container stack;
	gui_stack_init(&stack, (struct stack_opts) {
		STACK_OPTS_DEFAULTS,
		.vpadding = element_padding,
		.xoffset = border_padding,
		.yoffset = border_padding,
		.zoffset = 1
	});
	gui_container_push(&stack);

	gui_text("Bork", bold_text_opts);
	gui_stack_break(&stack);

	rg->cancel_btn_state = gui_btn(rg->cancel_btn_state, "Cancel", btn_opts);
	gui_stack_break(&stack);

	gui_container_pop();
	window_submitframe();
	return window.should_close;
}
