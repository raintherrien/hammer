#include "hammer/worldgen/worldgen.h"
#include "hammer/error.h"
#include "hammer/mem.h"
#include "hammer/worldgen/tectonic.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

static void worldgen_iteration(DL_TASK_ARGS);
static void worldgen_complete(DL_TASK_ARGS);

void
worldgen_create(struct worldgen_pkg *wg, unsigned long long seed, unsigned long size)
{
	if ((size_t)size > SIZE_MAX / size) {
		xperrorva("World size of %lu would cause size_t overflow", size);
		abort();
	}
	size_t area = size * size;
	float * restrict uplift =        xmalloc(area * sizeof(*uplift));
	float * restrict temperature =   xmalloc(area * sizeof(*temperature));
	float * restrict precipitation = xmalloc(area * sizeof(*precipitation));
	float * restrict elevation =     xmalloc(area * sizeof(*elevation));
	float * restrict wind_velocity = xmalloc(2 * area * sizeof(*wind_velocity));

	struct tectonic_uplift_opts opts = TECTONIC_UPLIFT_OPTS_DEFAULTS;
	opts.seed = seed;
	tectonic_uplift(uplift, &opts);
	/* TODO: junk values. large_scale_world_gen is a stub right now */
	float sunlight_latitude_extent_0 = 0.25f;
	float sunlight_latitude_extent_1 = 0.75f;

	*wg = (struct worldgen_pkg) {
		.task = DL_TASK_INIT(worldgen_iteration),
		.large_scale_world_gen_pkg = {
			.task = DL_TASK_INIT(large_scale_world_gen_iteration),
			.uplift = uplift,
			.sunlight_latitude_extent_0 = sunlight_latitude_extent_0,
			.sunlight_latitude_extent_1 = sunlight_latitude_extent_1,
			.temperature = temperature,
			.wind_velocity = wind_velocity,
			.precipitation = precipitation,
			.elevation = elevation,
			.seed = seed,
			.size = size
		},
		.iteration = 0
	};
}

void
worldgen_destroy(struct worldgen_pkg *wg)
{
	free(wg->large_scale_world_gen_pkg.uplift);
	free(wg->large_scale_world_gen_pkg.temperature);
	free(wg->large_scale_world_gen_pkg.precipitation);
	free(wg->large_scale_world_gen_pkg.elevation);
	free(wg->large_scale_world_gen_pkg.wind_velocity);
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
