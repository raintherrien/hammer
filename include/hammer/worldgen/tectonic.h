/*
 * This tectonic uplift simulation is loosely based upon:
 *   Lauri Viitanen, Physically Based Terrain Generation: Procedural Heightmap
 *   Generation Using Plate Tectonics, 2012.
 *   http://urn.fi/URN:NBN:fi:amk-201204023993
 */

#ifndef HAMMER_WORLDGEN_TECTONIC_H_
#define HAMMER_WORLDGEN_TECTONIC_H_

#include "hammer/well.h"
#include "hammer/worldgen/world_opts.h"
#include <stdint.h>

/* Any mass below ocean floor mass is not part of the map */
#define TECTONIC_OCEAN_FLOOR_MASS 0.1f
/* Any mass equal to or higher than continent mass is above sea level */
#define TECTONIC_CONTINENT_MASS   1.0f
/* Lithosphere dimensions */
#define LITHOSPHERE_SCALE 10
#define LITHOSPHERE_LEN  (1<<LITHOSPHERE_SCALE)
#define LITHOSPHERE_AREA (LITHOSPHERE_LEN * LITHOSPHERE_LEN)

/*
 * Limit area of segments to uint32_t and count to something reasonable so
 * that we can allocate a segment collision set.
 */
#define MAX_SEGMENT_AREA  (((size_t)1<<32)-1)
#define MAX_SEGMENT_COUNT 128
#define COLLISION_SET_BYTES (MAX_SEGMENT_COUNT * MAX_SEGMENT_COUNT / 8)
_Static_assert(MAX_SEGMENT_COUNT * MAX_SEGMENT_COUNT % 8 == 0);

#define NO_PLATE   ((uint32_t)-1)
#define NO_SEGMENT ((uint16_t)-1)

/*
 * An intra-plate collision identifies the lithosphere space coordinates of
 * the collision and the plate colliding. The plate colliding always collides
 * with whatever plate owns this cell.
 */
struct collision {
	uint32_t plate_index;
	uint32_t x, y;
};

/*
 * Segments are groups of continental cells. These can be large continents or
 * small island chains. The idea behind segmenting a plate is that when
 * continental land forms collide they merge rather than deform.
 *
 * The bounding box of each segment is stored in plate space to accelerate
 * blitting and transfers. Segments can be very small.
 */
struct segment {
	uint32_t area;
	uint32_t left, top;
	uint32_t w, h;
	uint16_t id;
};

/*
 * I've concocted these three fields to replace mass and extract rock
 * composition information. Mass of a cell is the sum of these fields.
 */
struct tectonic_mass_composition {
	float sediment;
	float metamorphic;
	float igneous;
};

struct plate {
	struct segment *segments;
	/*
	 * The original paper must have updated left/top whenever a plate
	 * shifted. This works when you have a 2D map of mass only as large as
	 * your plate (width x height), but since I'm trying to avoid resizing
	 * and shuffling that plate data I keep track of a translation vector.
	 * When going from "plate space" to "lithosphere space" you *add* the
	 * translation vector, and vice versa when going from "lithosphere
	 * space" to "plate space" you subtract the translation vector.
	 */
	float tvx, tvy;
	float tx, ty;
	struct tectonic_mass_composition mass[LITHOSPHERE_AREA];
	/*
	 * Do NOT attempt to define this using four points. Trust me. It's a
	 * total shitshow when you're dealing with iterating over wrapping
	 * coordinates. It was my go-to when I read the paper but now I
	 * understand why the author chose to represent coordinates this way!
	 */
	uint32_t left, top;
	uint32_t w, h;
	uint16_t in_segment[LITHOSPHERE_AREA];
};

struct lithosphere {
	struct collision *collisions;
	struct plate *plates;
	WELL512  rng;
	struct tectonic_mass_composition mass[LITHOSPHERE_AREA];
	uint32_t generation;
	uint32_t owner[LITHOSPHERE_AREA];
	uint32_t prev_owner[LITHOSPHERE_AREA];
	uint8_t  collision_set[COLLISION_SET_BYTES];
};

void lithosphere_create(struct lithosphere *, const struct world_opts *);
void lithosphere_destroy(struct lithosphere *);
void lithosphere_update(struct lithosphere *, const struct world_opts *);
void lithosphere_blit(struct lithosphere *l, float *uplift, unsigned long size);

#endif /* HAMMER_WORLDGEN_TECTONIC_H_ */
