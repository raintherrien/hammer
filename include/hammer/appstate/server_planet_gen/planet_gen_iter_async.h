#ifndef HAMMER_APPSTATE_SERVER_PLANET_GEN_ITER_ASYNC_H_
#define HAMMER_APPSTATE_SERVER_PLANET_GEN_ITER_ASYNC_H_

#include <deadlock/dl.h>
#include <GL/glew.h>
#include <stdatomic.h>

/*
 * Planet generation is an iterative process which steps through each of these
 * stages in order.
 */
enum planet_gen_iter_stage {
	PLANET_STAGE_NONE,
	PLANET_STAGE_LITHOSPHERE,
	PLANET_STAGE_CLIMATE,
	PLANET_STAGE_STREAM,
	PLANET_STAGE_COMPOSITE
};

/*
 * server_update_async iterates whatever stage of planet generation we're in
 * and computes an image of the result to be blitted by the render thread.
 *
 * This runs asynchronously from the render thread to keep the UI frame time
 * reasonable, and iteratively so that the render thread can provide
 * interactive feedback to the user.
 *
 * TODO: I need to consider whether I want to wrap dltask in some kind of
 * coroutine abstraction because I suspect this 'is_running' and 'can_resume'
 * pattern to be prevalent.
 */
struct planet_gen_iter_async {
	dltask task;

	/* Written to while running, readable by render thread after */
	GLubyte *iteration_render;
	int      iteration_render_width_height;

	/* Signal that an iteration is complete and img can be read */
	atomic_int is_running;

	/* Which stage was last blitted to img */
	enum planet_gen_iter_stage last_stage;

	/* Next iteration stage */
	enum planet_gen_iter_stage next_stage;

	/* Whether there are more iterations to perform */
	int can_resume;
};

void planet_gen_iter_async_create (struct planet_gen_iter_async *);
void planet_gen_iter_async_destroy(struct planet_gen_iter_async *);
void planet_gen_iter_async_resume (struct planet_gen_iter_async *);

#endif /* HAMMER_APPSTATE_SERVER_PLANET_GEN_ITER_ASYNC_H_ */
