#ifndef HAMMER_WORLDGEN_LARGE_SCALE_WORLD_GEN_H_
#define HAMMER_WORLDGEN_LARGE_SCALE_WORLD_GEN_H_

#include "hammer/worldgen/climate.h"
#include <stddef.h>
#include <stdint.h>
#include <deadlock/dl.h>

/*
 * Large scale world generation is a feedback system of tectonic uplift,
 * erosion, and climate simulation.
 *
 * Tectonic uplift and fluvial/thermal erosion is based upon the stream tree
 * representation and erosion model described in:
 *   Guillaume Cordonnier, Jean Braun, Marie-Paule Cani, Bedrich Benes, Eric
 *   Galin, et al.. Large Scale Terrain Generation from Tectonic Uplift and
 *   Fluvial Erosion. Computer Graphics Forum, Wiley, 2016, Proc. EUROGRAPHICS
 *   2016, 35 (2), pp.165-175. 10.1111/cgf.12820. hal-01262376
 *
 * Climate modeling is cobbled together crap.
 */

struct large_scale_world_gen_state;

struct large_scale_world_gen_pkg {
	dltask task;
	/*
	 * Plate tectonics generate tectonic uplift. This is the rate of
	 * change of elevation before applying erosion.
	 */
	float * restrict uplift;
	/* TODO: Document */
	struct climate c;
	/*
	 * Elevation is increased by tectonic uplift and reduced by erosion.
	 * Fluvial and thermal erosion are functions of precipitation and
	 * temperature.
	 */
	float * restrict elevation;
	/*
	 * An opaque state owned but this pkg and used by each iteration.
	 * Allocated by large_scale_world_gen_create and freed by _destroy.
	 */
	struct large_scale_world_gen_state *state;

	unsigned long long seed;
	unsigned long size;
};

void large_scale_world_gen_create(struct large_scale_world_gen_pkg *);
void large_scale_world_gen_destroy(struct large_scale_world_gen_pkg *);
void large_scale_world_gen_iteration(DL_TASK_ARGS);

#endif /* HAMMER_WORLDGEN_LARGE_SCALE_WORLD_GEN_H_ */
