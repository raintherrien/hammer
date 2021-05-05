#ifndef HAMMER_WORLDGEN_TECTONIC_H_
#define HAMMER_WORLDGEN_TECTONIC_H_

#include "hammer/well.h"
#include <stdint.h>

/* Any mass below ocean floor mass is not part of the map */
#define TECTONIC_OCEAN_FLOOR_MASS 0.1f
/* Any mass equal to or higher than continent mass is above sea level */
#define TECTONIC_CONTINENT_MASS   1.0f
/* Lithosphere dimensions */
#define LITHOSPHERE_SCALE 10
#define LITHOSPHERE_LEN  (1<<LITHOSPHERE_SCALE)
#define LITHOSPHERE_AREA (LITHOSPHERE_LEN * LITHOSPHERE_LEN)

struct tectonic_opts {
	unsigned long long seed;
	/*
	 * Collision and subduction xfer factors define how much mass is
	 * transfered when a plate subducts under or collides with another
	 * plate every generation step.
	 */
	float collision_xfer;
	float subduction_xfer;
	/*
	 * The ratio of area a segment may be overlapped before being merged
	 * onto the larger segment. This value directly affects mountain range
	 * width.
	 */
	float merge_ratio;
	/*
	 * Mass of a divergent boundary rift, created at divergent boundary
	 * cells every rift_ticks steps.
	 *
	 * Volcanos are a special type of rift cell, only created randomly.
	 * Each rift cell has a volcano_chance:1 chance of becoming a volcano.
	 */
	float rift_mass;
	float volcano_mass;
	float volcano_chance;
	/*
	 * Continent and ocean talus slopes (not angles) determine the effect
	 * of erosion. Each erosion iteration mass is moved within a plate
	 * from a cell to all its lower neighbors until the slope does not
	 * exceed the material talus angle.
	 *
	 * In the real world erosion has less effect under water, but I find
	 * a higher ocean_talus looks better.
	 */
	float continent_talus;
	float ocean_talus;
	/*
	 * Each generation segments the map into between min- and max-plates
	 * and iterates a certain number of steps. When plates are recycled
	 * any plates currently subducting are discarded, so more generations
	 * results in more island-ey worlds.
	 */
	unsigned generation_steps;
	unsigned generations;
	unsigned max_plates;
	unsigned min_plates;
	/*
	 * Segment radius is how far each cell of land will search within its
	 * plate for a neighbor with land to become a segment. A higher value
	 * will group distant cells of land together into segments. This can
	 * form interesting, small island chains off the coasts of continents.
	 */
	unsigned segment_radius;
	/*
	 * Divergent radius is how far each unowned divergent cell searches
	 * for its heighest neighbor to determine the height of the rift,
	 * which is a quadratic function.
	 */
	unsigned divergent_radius;
	/*
	 * Every step % erosion_ticks == 0 erosion is performed to smooth
	 * steep edges.
	 */
	unsigned erosion_ticks;
	/*
	 * Every step % rift_ticks == 0 a divergent boundary rift is created
	 * in unowned divergent boundary cells. This should be a multiple of
	 * erosion_ticks or else rifts may be too steep.
	 */
	unsigned rift_ticks;
};

#define TECTONIC_OPTS_DEFAULTS        \
	.collision_xfer   = 0.0125f,  \
	.subduction_xfer  = 0.0125f,  \
	.merge_ratio      = 0.125f,   \
	.rift_mass        = 0.9f,     \
	.volcano_mass     = 15.0f,    \
	.volcano_chance   = 0.005f,   \
	.continent_talus  = 0.06f,    \
	.ocean_talus      = 0.05f,    \
	.generation_steps = 100,      \
	.generations      = 2,        \
	.min_plates       = 10,       \
	.max_plates       = 25,       \
	.segment_radius   = 2,        \
	.divergent_radius = 5,        \
	.erosion_ticks    = 20,       \
	.rift_ticks       = 60

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
	float mass[LITHOSPHERE_AREA];
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
	float    mass[LITHOSPHERE_AREA];
	uint32_t generation;
	uint32_t owner[LITHOSPHERE_AREA];
	uint32_t prev_owner[LITHOSPHERE_AREA];
	uint8_t  collision_set[COLLISION_SET_BYTES];
};

void lithosphere_create(struct lithosphere *, const struct tectonic_opts *);
void lithosphere_destroy(struct lithosphere *);
void lithosphere_update(struct lithosphere *, const struct tectonic_opts *);
void lithosphere_blit(struct lithosphere *l, float *uplift, unsigned long size);

#endif /* HAMMER_WORLDGEN_TECTONIC_H_ */
