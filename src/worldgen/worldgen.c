#include "hammer/worldgen/worldgen.h"
#include "hammer/error.h"
#include "hammer/mem.h"
#include "hammer/worldgen/climate.h"
#include "hammer/worldgen/tectonic.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

static void worldgen_iteration(DL_TASK_ARGS);
static void worldgen_complete(DL_TASK_ARGS);

void
worldgen_create(struct worldgen_pkg *wg, unsigned long long seed, unsigned long size)
{
	if ((size_t)size > SIZE_MAX / size)
		xpanicva("World size of %lu would cause size_t overflow", size);

	struct tectonic_opts opts = {
		TECTONIC_OPTS_DEFAULTS,
		.seed = seed
	};
	float *uplift = tectonic_uplift(&opts, size);
	struct climate c = generate_climate(uplift, size);

	*wg = (struct worldgen_pkg) {
		.task = DL_TASK_INIT(worldgen_iteration),
		.large_scale_world_gen_pkg = {
			.task = DL_TASK_INIT(large_scale_world_gen_iteration),
			.uplift = uplift,
			.c = c,
			.elevation = NULL,
			.seed = seed,
			.size = size
		},
		.iteration = 0
	};
}

void
worldgen_destroy(struct worldgen_pkg *wg)
{
	/* TODO: Obviously doesn't belong here */
	free(wg->large_scale_world_gen_pkg.uplift);
	free(wg->large_scale_world_gen_pkg.c.moisture);
	free(wg->large_scale_world_gen_pkg.c.precipitation);
	free(wg->large_scale_world_gen_pkg.c.pressure);
	free(wg->large_scale_world_gen_pkg.c.pressure_flow);
	free(wg->large_scale_world_gen_pkg.c.temperature);
	free(wg->large_scale_world_gen_pkg.c.wind_velocity);
	free(wg->large_scale_world_gen_pkg.elevation);
}

static void
worldgen_iteration(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct worldgen_pkg, wg, task);

	++ wg->iteration;
	if (wg->iteration < 512)
		dlcontinuation(&wg->task, worldgen_iteration);
	else
		dlcontinuation(&wg->task, worldgen_complete);
	dlnext(&wg->large_scale_world_gen_pkg.task, &wg->task);
	dlwait(&wg->task, 1);
	dlasync(&wg->large_scale_world_gen_pkg.task);
}

static void
worldgen_complete(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct worldgen_pkg, wg, task);
	puts("World generation complete");
}
