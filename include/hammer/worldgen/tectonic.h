#ifndef HAMMER_WORLDGEN_TECTONIC_H_
#define HAMMER_WORLDGEN_TECTONIC_H_

/* Any mass below ocean floor mass is not part of the map */
#define TECTONIC_OCEAN_FLOOR_MASS 0.1f
/* Any mass equal to or higher than continent mass is above sea level */
#define TECTONIC_CONTINENT_MASS   1.0f

struct tectonic_uplift_opts {
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

#define TECTONIC_UPLIFT_OPTS_DEFAULTS \
(struct tectonic_uplift_opts) {       \
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
	.rift_ticks       = 60,       \
}

void tectonic_uplift(float *uplift, const struct tectonic_uplift_opts *);

#endif /* HAMMER_WORLDGEN_TECTONIC_H_ */
