#include "hammer/math.h"
#include "hammer/mem.h"
#include "hammer/opensimplex.h"
#include "hammer/ring.h"
#include "hammer/vector.h"
#include "hammer/worldgen/tectonic.h"
#include <assert.h>
#include <float.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define MIN_XFER 0.0000125f

/*
 * This tectonic uplift simulation is loosely based upon:
 *   Lauri Viitanen, Physically Based Terrain Generation: Procedural Heightmap
 *   Generation Using Plate Tectonics, 2012.
 *   http://urn.fi/URN:NBN:fi:amk-201204023993
 *
 * I settled on something that resembles the outlined program structure for
 * clarity when referring back to the paper, however I have not attempted to
 * recreate the solution described. The objective of Viitanen's thesis was to
 * model the process of plate tectonics, whereas my goal is to approximate the
 * result of plate tectonics.
 *
 * Quite a few aspects of the described program structure stuck out as odd to
 * me when I first read the paper, such as left/top based coordinates, which I
 * typically associate with GUIs and window managers. After some experimenting
 * I realized why this was done and it does make much more sense.
 *
 * However, this program grew organically through a combination of
 * experimentation and skimming the source paper. Notable differences include:
 *
 * - I assume crust age in the paper was a density modifier, however it does
 *   not go in depth about implementation. I omitted it when drafting a
 *   prototype and never went back to implement it.
 *   TODO: eventually I would like to add this back in, to feed rock types to
 *         later stages of the pipeline for a little variety.
 *
 * - My preliminary attempts used only map data at the lithosphere level,
 *   where during each step when plates were moved around data was translated
 *   from a front buffer to a back-buffer. In retrospect, this obviously does
 *   not work if you want to have information about plates currently
 *   overlapping, and that's half the point of a tectonic sim! Otherwise all
 *   boundaries are at most ||velocity|| wide, which would only kind of work
 *   at very low resolutions. We must keep an elevation map per plate like the
 *   program structure described in the paper.
 *
 *   However, the paper seems to imply each plate map is only as large as the
 *   plate (width x height). This is a clever way to save memory, but all the
 *   allocations involved with growing or moving a plate, as well as the
 *   bookkeeping to determine which direction a plate is growing, made me veer
 *   in a different direction. I would much rather eat the 6-8MiB/plate (most
 *   of which is wasted, yes) and give each plate its own map the size of our
 *   world.
 *
 * - I did not implement velocity as a translation + rotation matrix because
 *   my decision above makes it impossible to practically determine the center
 *   of a plate. I don't find the results lacking enough to deal with the
 *   complexity mentioned above.
 *
 * - I use a fixed map size for simplicity. This allows us to use uint32_t
 *   where the paper uses size_t. Any use of this tectonic map is going to be
 *   blurred and interpolated to shit anyway so this is plenty of detail for
 *   me. This also allows us to define our arrays with a constant size rather
 *   than rely on nested allocations, which is just nice and will compile
 *   better if we keep size a power of two (no modulo).
 *
 * - Certain features, like friction and physics between plates I experimented
 *   with but didn't find the complexity worthwhile.
 *
 * - I handle collisions very inefficiently but this part of the paper makes
 *   no sense to me. This can be worked around with collision list ordering
 *   and memoization and I'm happy enough with it.
 */

/*
 * Populates a divergent cell with no owner with magma and assignes a plate.
 */
static void divergent_cell(struct lithosphere *, uint32_t,
                           const struct tectonic_opts *);

/*
 * Subdivides current mass map into a new set of plates. Note: any currently
 * subducting/colliding plates are discarded, such that more calls to this
 * function results in more mass loss throughout the map.
 */
static void lithosphere_create_plates(struct lithosphere *,
                                      const struct tectonic_opts *);

/*
 * Initializes the mass map with coherent wrapping noise.
 */
static void lithosphere_init_mass(struct lithosphere *);

/*
 * Performs one iteration of the tectonic uplift algorithm.
 */
static void lithosphere_update_impl(struct lithosphere *,
                               const struct tectonic_opts *);

/*
 * Erodes mass within plate boundary.
 */
static void plate_erode(struct plate *, const struct tectonic_opts *);

/*
 * Transfers all mass from the source segment in the source plate to the
 * destination segment in the destination plate. in_segment and owner are
 * updated.
 */
static void plate_swap_segment(struct lithosphere *l,
                               uint32_t src_pi, uint32_t dst_pi,
                               uint16_t src_si, uint16_t dst_si);

/*
 * Blit plate mass onto lithosphere, perform subduction and collision mass
 * transfer and identify collisions which may result in segments merging.
 */
static void plate_blit(struct lithosphere *, uint32_t pi,
                       const struct tectonic_opts *);

/*
 * Frees memory associated with a plate.
 */
static void plate_destroy(struct plate *);

/*
 * Assigns a random initial velocity to a plate.
 */
static void plate_init_velocity(struct plate *, WELL512);

/*
 * plate_recalculate_segments() subdivides the plate's continental mass into
 * segments. See tectonic_opts.segment_radius for details.
 *
 * plate_growbfs_segment() is the breadth-first search component of finding
 * segments.
 */
static void plate_growbfs_segment(struct plate *, uint32_t **bfs, uint16_t si,
                                  const struct tectonic_opts *);
static void plate_segment(struct plate *, uint16_t *sid_ter,
                          const struct tectonic_opts *);

/*
 * Sets or adds crust to a plate, and assigns that mass to a segment, using
 * world coordinates. TODO: the add_not_overwrite param is me being lazy.
 */
static void plate_set_mass(struct plate *, uint32_t wx, uint32_t wy,
                           int add_not_overwrite, float mass, uint16_t si);

/*
 * Calculates the total area two segments overlap. This is a very expensive
 * operation.
 */
static uint32_t segment_overlap(struct plate *p0, struct plate *p1,
                                uint16_t si0, uint16_t si1);

/*
 * Utility functions to wrap a potentially negative coordinate to the bounds
 * of a lithosphere map and convert between lithosphere and plate coordinates.
 */
static inline long
wrap(long X)
{
	while (X < 0)
		X += LITHOSPHERE_LEN;
	return X % LITHOSPHERE_LEN;
}

static inline void
lithosphere_to_plate(struct plate *p, uint32_t lx, uint32_t ly, uint32_t pxy[2])
{
	pxy[0] = wrap(lroundf(lx + p->tx));
	pxy[1] = wrap(lroundf(ly + p->ty));
}

static inline void
plate_to_lithosphere(struct plate *p, uint32_t px, uint32_t py, uint32_t lxy[2])
{
	lxy[0] = wrap(lroundf(px - p->tx));
	lxy[1] = wrap(lroundf(py - p->ty));
}

void
lithosphere_create(struct lithosphere *l, const struct tectonic_opts *opts)
{
	memset(l, 0, sizeof(*l));
	WELL512_seed(l->rng, opts->seed);
	lithosphere_init_mass(l);
}

void
lithosphere_destroy(struct lithosphere *l)
{
	vector_free(&l->collisions);
	size_t plate_count = vector_size(l->plates);
	for (uint32_t p = 0; p < plate_count; ++ p)
		plate_destroy(l->plates + p);
	vector_free(&l->plates);
}

void
lithosphere_update(struct lithosphere *l, const struct tectonic_opts *opts)
{
	if (l->generation % opts->generation_steps == 0) {
		size_t plate_count = vector_size(l->plates);
		for (uint32_t p = 0; p < plate_count; ++ p)
			plate_destroy(l->plates + p);
		vector_clear(&l->plates);
		lithosphere_create_plates(l, opts);
	}
	lithosphere_update_impl(l, opts);
}

void
lithosphere_blit(struct lithosphere *l, float *uplift, unsigned long size)
{
	struct opensimplex *n[6] = {
		opensimplex_alloc(WELL512i(l->rng)),
		opensimplex_alloc(WELL512i(l->rng)),
		opensimplex_alloc(WELL512i(l->rng)),
		opensimplex_alloc(WELL512i(l->rng)),
		opensimplex_alloc(WELL512i(l->rng)),
		opensimplex_alloc(WELL512i(l->rng))
	};
	const float s = size / LITHOSPHERE_LEN;
	const float wf = 1.0f / 15;
	const float frq = 24;
	for (uint32_t y = 0; y < size; ++ y)
	for (uint32_t x = 0; x < size; ++ x) {
		/* See lithosphere_init_mass for an explanation */
		size_t i = y * size + x;
		float u = (float)x / size;
		float v = (float)y / size;
		float nx = cosf(u*2*M_PI)*frq/(2*M_PI);
		float ny = sinf(v*2*M_PI)*frq/(2*M_PI);
		float nz = sinf(u*2*M_PI)*frq/(2*M_PI);
		float nw = cosf(v*2*M_PI)*frq/(2*M_PI);
		nx += opensimplex4_fbm(n[0], nx*wf, ny*wf, nz*wf, nw*wf, 3, 4);
		ny += opensimplex4_fbm(n[1], nx*wf, ny*wf, nz*wf, nw*wf, 3, 4);
		nz += opensimplex4_fbm(n[2], nx*wf, ny*wf, nz*wf, nw*wf, 3, 4);
		nw += opensimplex4_fbm(n[3], nx*wf, ny*wf, nz*wf, nw*wf, 3, 4);
		float wx = opensimplex4_fbm(n[4], nx, ny, nz, nw, 6, 2);
		float wy = opensimplex4_fbm(n[5], nx, ny, nz, nw, 6, 2);
		size_t iw = wrap(lroundf(y / s + frq * wx)) * LITHOSPHERE_LEN +
		            wrap(lroundf(x / s + frq * wy));
		uplift[i] = l->mass[iw];
	}
	for (size_t i = 0; i < 6; ++ i)
		free(n[i]);
}

static void
divergent_cell(struct lithosphere *l, uint32_t i,
               const struct tectonic_opts *opts)
{
	uint32_t po = l->prev_owner[i];
	uint32_t x = i % LITHOSPHERE_LEN;
	uint32_t y = i / LITHOSPHERE_LEN;
	float h = TECTONIC_OCEAN_FLOOR_MASS;
	long r = opts->divergent_radius;
	for (long dy = -r; dy <= r; ++ dy)
	for (long dx = -r; dx <= r; ++ dx) {
		if (dy == 0 && dx == 0)
			continue;
		uint32_t n = wrap(dy + y) * LITHOSPHERE_LEN + wrap(dx + x);
		float nh = (l->mass[n] - TECTONIC_OCEAN_FLOOR_MASS) / (TECTONIC_CONTINENT_MASS - TECTONIC_OCEAN_FLOOR_MASS);
		if (nh > 1)
			nh = 1;
		h = MAX(h, nh);
	}
	h = powf(h-0.1f,2) + powf(0.5f-h,3);
	float new_mass = TECTONIC_OCEAN_FLOOR_MASS + TECTONIC_CONTINENT_MASS * h;
	if (l->generation % opts->rift_ticks == (opts->rift_ticks-1)) {
		if (WELL512f(l->rng) <= opts->volcano_chance)
			new_mass = opts->volcano_mass;
		else
			new_mass = opts->rift_mass;
	}
	l->owner[i] = po;
	plate_set_mass(l->plates + po, x, y, 0, new_mass, NO_SEGMENT);
}

static void
lithosphere_create_plates(struct lithosphere *l,
                          const struct tectonic_opts *opts)
{
	/* Assign each cell an owner plate */
	memset(l->owner, 0xFF, LITHOSPHERE_AREA * sizeof(*l->owner));
	/* Identify cells still growing using a ring buffer */
	_Static_assert((size_t)((uint32_t)-1) >= LITHOSPHERE_AREA);
	uint32_t *growing = NULL;

	/* Create initial plates */
	size_t plate_count = opts->min_plates +
	                     (WELL512i(l->rng) % (opts->max_plates -
	                                          opts->min_plates));
	for (uint32_t pi = 0; pi < plate_count; ++ pi) {
		struct plate *p = vector_pushz(&l->plates);
		reclaim: ;
		uint32_t claim = WELL512i(l->rng) % LITHOSPHERE_AREA;
		if (l->owner[claim] != NO_PLATE)
			goto reclaim; /* already claimed */
		/* claim and mark as growing */
		l->owner[claim] = pi;
		ring_push(&growing, claim);
		/* set initial plate dimensions */
		p->left = claim % LITHOSPHERE_LEN;
		p->top  = claim / LITHOSPHERE_LEN;
		p->w  = 1;
		p->h = 1;
	}

	/*
	 * In order to fuzz our plates up a little bit we claim at most one
	 * new cell for each plate per iteration, only advancing the to_grow
	 * minimum index when all four neighbors of that cell are a no-go.
	 * This means we loop over growing quite a few more times than is
	 * necessarily, but I believe at most 4 times? Can't math too tired.
	 *
	 * This is more-or-less fair, like the algorithm described in the
	 * paper, but being perfectly fair to each plate would be a real pain
	 * in the ass. :)
	 */
	while (ring_size(growing)) {
		size_t it = 0;
		uint32_t *el;
		while ((el = ring_iter(growing,it))) {
			uint32_t src = *el;
			uint32_t x = src % LITHOSPHERE_LEN;
			uint32_t y = src / LITHOSPHERE_LEN;
			/* Von Neumann neighbors, wrap edges */
			uint32_t xm = x == 0 ? LITHOSPHERE_LEN-1 : x - 1;
			uint32_t xp = x == LITHOSPHERE_LEN-1 ? 0 : x + 1;
			uint32_t ym = y == 0 ? LITHOSPHERE_LEN-1 : y - 1;
			uint32_t yp = y == LITHOSPHERE_LEN-1 ? 0 : y + 1;
			/* Note: order of neighbors matters below! */
			uint32_t neighbors[4] = {
				y * LITHOSPHERE_LEN + xm,
				y * LITHOSPHERE_LEN + xp,
				ym * LITHOSPHERE_LEN + x,
				yp * LITHOSPHERE_LEN + x
			};
			/* Not very random. Will always access in pattern */
			size_t rng_offset = WELL512i(l->rng) % 128;
			for (size_t n = 0; n < 4; ++ n) {
				size_t i = (n + rng_offset) % 4;
				uint32_t neighbor = neighbors[i];
				if (l->owner[neighbor] != NO_PLATE)
					continue;
				/* claim and mark as growing */
				l->owner[neighbor] = l->owner[src];
				ring_push(&growing, neighbor);
				/* grow plate dimensions */
				struct plate *p = l->plates + l->owner[src];
				uint32_t r = wrap((long)p->left + p->w - 1);
				uint32_t b = wrap((long)p->top + p->h - 1);
				if (i == 0 && p->left == x) {
					p->left = wrap((long)p->left-1);
					++ p->w;
				} else if (i == 2 && p->top == y) {
					p->top = wrap((long)p->top-1);
					++ p->h;
				} else if (i == 1 && r == x) {
					++ p->w;
				} else if (i == 3 && b == y) {
					++ p->h;
				}
				goto neighbor_claimed;
			}
			/*
			 * If we didn't claim a neighbor, don't grow from this
			 * cell again. Advance our minimum iterator. If we're
			 * not currently the bottom of the list don't worry!
			 * We'll get rolled up when everyone before us does.
			 * TODO: This made sense once, now it needs a doubly-
			 * linked list.
			 */
			if (it == 0) {
				ring_pop(&growing);
				continue;
			}
			neighbor_claimed:
				++ it;
		}
	}

	/* Copy mass data to plates */
	for (uint32_t pi = 0; pi < plate_count; ++ pi) {
		struct plate *p = l->plates + pi;
		for (uint32_t y = 0; y < p->h; ++ y)
		for (uint32_t x = 0; x < p->w;  ++ x) {
			size_t i = wrap(p->top  + y) * LITHOSPHERE_LEN +
			           wrap(p->left + x);
			if (l->owner[i] == pi)
				p->mass[i] = l->mass[i];
		}
		plate_init_velocity(p, l->rng);
	}

	ring_free(&growing);
}

static void
lithosphere_init_mass(struct lithosphere *l)
{
	struct opensimplex *n[5] = {
		opensimplex_alloc(WELL512i(l->rng)),
		opensimplex_alloc(WELL512i(l->rng)),
		opensimplex_alloc(WELL512i(l->rng)),
		opensimplex_alloc(WELL512i(l->rng)),
		opensimplex_alloc(WELL512i(l->rng))
	};
	const float wf = 0.25f;
	const float frq = 9.5f;

	for (uint32_t y = 0; y < LITHOSPHERE_LEN; ++ y)
	for (uint32_t x = 0; x < LITHOSPHERE_LEN; ++ x) {
		size_t i = y * LITHOSPHERE_LEN + x;
		/*
		 * To generate coherent wrapping noise we may project our
		 * coordinates onto unit circles along orthogonal axis.
		 *
		 * Once you've attempted to do this in 2D you'll realize your
		 * axis are warping due to, well, sine and cosine. This would
		 * be a fine solution for a 1D noise image. What you actually
		 * need are complementing 3rd and 4th axis, the same way sine
		 * and cosine complement eachother for the 1D case.
		 *
		 * Once we have these cyclical 4D coordinates we can warp,
		 * transform, and scale them just like our linear 2D
		 * coordinates.
		 *
		 * TODO: This took me 48.7 years to massage into something
		 * that works. Would be smart to turn it into a function.
		 */
		float u = (float)x / LITHOSPHERE_LEN;
		float v = (float)y / LITHOSPHERE_LEN;
		float nx = cosf(u*2*M_PI)*frq/(2*M_PI);
		float ny = sinf(v*2*M_PI)*frq/(2*M_PI);
		float nz = sinf(u*2*M_PI)*frq/(2*M_PI);
		float nw = cosf(v*2*M_PI)*frq/(2*M_PI);
		nx += opensimplex4_fbm(n[0], nx*wf, ny*wf, nz*wf, nw*wf, 3, 4);
		ny += opensimplex4_fbm(n[1], nx*wf, ny*wf, nz*wf, nw*wf, 3, 4);
		nz += opensimplex4_fbm(n[2], nx*wf, ny*wf, nz*wf, nw*wf, 3, 4);
		nw += opensimplex4_fbm(n[3], nx*wf, ny*wf, nz*wf, nw*wf, 3, 4);
		l->mass[i] = opensimplex4_fbm(n[4], nx, ny, nz, nw, 6, 2);
		l->mass[i] = 1.2f + 1.5f * l->mass[i];
	}
	for (size_t i = 0; i < LITHOSPHERE_AREA; ++ i)
		if (l->mass[i] < TECTONIC_OCEAN_FLOOR_MASS)
			l->mass[i] = TECTONIC_OCEAN_FLOOR_MASS;
	for (size_t i = 0; i < 5; ++ i)
		free(n[i]);
}

static void
lithosphere_update_impl(struct lithosphere *l,
                        const struct tectonic_opts *opts)
{
	++ l->generation;

	size_t plate_count = vector_size(l->plates);
	for (uint32_t pi = 0; pi < plate_count; ++ pi) {
		struct plate *p = l->plates + pi;

		/* Apply plate velocity. */
		p->tx += p->tvx;
		p->ty += p->tvy;
	}

	/*
	 * Now that our plates have moved, clear lithosphere owner and mass
	 * maps and blit plate-level information onto the lithosphere,
	 * identifying inter-plate collisions where two plates try to claim
	 * ownership.
	 */
	memcpy(l->prev_owner, l->owner, LITHOSPHERE_AREA * sizeof(uint32_t));
	memset(l->mass, 0, LITHOSPHERE_AREA * sizeof(*l->mass));
	memset(l->owner, 0xFF, LITHOSPHERE_AREA * sizeof(*l->owner));

	/* Segment plates before creating collisions */
	uint16_t sid_ter = 0;
	memset(l->collision_set, 0, COLLISION_SET_BYTES);
	for (uint32_t pi = 0; pi < plate_count; ++ pi)
		plate_segment(l->plates + pi, &sid_ter, opts);

	/* Blit plate information to the lithosphere, identify collisions */
	for (uint32_t pi = 0; pi < plate_count; ++ pi)
		plate_blit(l, pi, opts);

	/*
	 * Resolve collisions. Note: this has not been packed away in its own
	 * little function because I find it useful to eliminate duplicate
	 * calls to segment_overlap using cache variables. TODO: The smarter
	 * thing to do would be pre-process l->collisions to eliminate
	 * duplicates before this point.
	 */
	struct segment *last_s0 = NULL, *last_s1 = NULL;
	size_t collision_count = vector_size(l->collisions);
	for (size_t i = 0; i < collision_count; ++ i) {
		struct collision *c = l->collisions + i;
		size_t dst = c->y * LITHOSPHERE_LEN + c->x;
		if (c->plate_index == l->owner[dst])
			continue;
		struct plate *p0 = l->plates + c->plate_index;
		struct plate *p1 = l->plates + l->owner[dst];
		uint32_t p0xy[2];
		uint32_t p1xy[2];
		lithosphere_to_plate(p0, c->x, c->y, p0xy);
		lithosphere_to_plate(p1, c->x, c->y, p1xy);
		size_t i0 = p0xy[1] * LITHOSPHERE_LEN + p0xy[0];
		size_t i1 = p1xy[1] * LITHOSPHERE_LEN + p1xy[0];
		uint16_t si0 = p0->in_segment[i0];
		uint16_t si1 = p1->in_segment[i1];
		if (si0 != NO_SEGMENT && si1 != NO_SEGMENT) {
			struct segment *s0 = p0->segments + si0;
			struct segment *s1 = p1->segments + si1;
			if (last_s0 == s0 && last_s1 == s1) {
				/* Don't re-calc overlap */
				continue;
			}
			last_s0 = s0;
			last_s1 = s1;
			float overlap = segment_overlap(p0, p1, si0, si1);
			float smaller_area = MIN(s0->area, s1->area);
			/*
			 * If our segments overlap more than some ratio, merge
			 * the smaller onto the larger.
			 */
			if (overlap / smaller_area >= opts->merge_ratio) {
				if (s0->area > s1->area)
					plate_swap_segment(l, l->owner[dst], c->plate_index, si1, si0);
				else
					plate_swap_segment(l, c->plate_index, l->owner[dst], si0, si1);
			}
		}
	}
	vector_clear(&l->collisions);

	/*
	 * Any cell that isn't claimed at this point is a divergent boundary.
	 * Divergent boundaries are typically ridges, so we populate this area
	 * with more mass than the ocean floor and assign it to whichever
	 * plate owned this cell before moving.
	 *
	 * TODO: This function for determining the mass (elevation) of magma
	 * flowing from a divergent boundary is pretty nonsense and broken,
	 * but once eroded I actually like the janky effect. It's not at all
	 * what I had intended.
	 */
	for (uint32_t i = 0; i < LITHOSPHERE_AREA; ++ i)
		if (l->owner[i] == NO_PLATE)
			divergent_cell(l, i, opts);

	/* Apply erosion */
	if (l->generation % opts->erosion_ticks == 0)
		for (uint32_t pi = 0; pi < plate_count; ++ pi)
			plate_erode(l->plates + pi, opts);
}

static void
plate_erode(struct plate *p, const struct tectonic_opts *opts)
{
	/* Very basic erosion */
	for (uint32_t y = 0; y < p->h; ++ y)
	for (uint32_t x = 0; x < p->w;  ++ x) {
		size_t i = wrap(p->top  + y) * LITHOSPHERE_LEN +
			   wrap(p->left + x);
		float h = p->mass[i];
		if (h <= 0)
			continue;
		uint32_t ni[8] = {
			wrap(p->top+y-1) * LITHOSPHERE_LEN + wrap(p->left+x-1),
			wrap(p->top+y-1) * LITHOSPHERE_LEN + wrap(p->left+x+0),
			wrap(p->top+y-1) * LITHOSPHERE_LEN + wrap(p->left+x+1),
			wrap(p->top+y+0) * LITHOSPHERE_LEN + wrap(p->left+x-1),
			wrap(p->top+y+0) * LITHOSPHERE_LEN + wrap(p->left+x+1),
			wrap(p->top+y+1) * LITHOSPHERE_LEN + wrap(p->left+x-1),
			wrap(p->top+y+1) * LITHOSPHERE_LEN + wrap(p->left+x+0),
			wrap(p->top+y+1) * LITHOSPHERE_LEN + wrap(p->left+x+1)
		};
		float nh[8];
		float talus = h < TECTONIC_CONTINENT_MASS
		                ? opts->ocean_talus
		                : opts->continent_talus;
		for (size_t i = 0; i < 8; ++ i) {
			size_t n = ni[i];
			if (p->mass[n] <= 0) {
				nh[i] = 0;
			}
			/* Erode land into ocean at half rate */
			else if (h >= TECTONIC_CONTINENT_MASS &&
			         p->mass[n] < TECTONIC_CONTINENT_MASS)
			{
				nh[i] = (h - p->mass[n] - talus) / 2;
			} else {
				nh[i] = h - p->mass[n] - talus;
			}
		}
		float dh = 0; /* delta to lowest neighbor */
		float da = 0; /* accumulate deltas to all neighbors */
		for (size_t i = 0; i < 8; ++ i) {
			if (nh[i] > 0) {
				da += nh[i];
				dh = MAX(dh, nh[i]);
			}
		}
		if (dh < 0)
			continue;
		dh /= 2; /* half to prevent oscillation */
		if (da > dh) {
			da = dh / da;
		}
		if (dh > FLT_EPSILON) {
			p->mass[i] -= dh;
			for (size_t i = 0; i < 8; ++ i) {
				size_t n = ni[i];
				if (nh[i] > 0)
					p->mass[n] += nh[i] * da;
			}
		}

	}
}

static void
plate_swap_segment(struct lithosphere *l,
                   uint32_t src_pi, uint32_t dst_pi,
                   uint16_t src_si, uint16_t dst_si)
{
        struct plate *src = l->plates + src_pi;
        struct plate *dst = l->plates + dst_pi;
	struct segment *src_segment = src->segments + src_si;
	for (uint32_t sy = 0; sy < src_segment->h; ++ sy)
	for (uint32_t sx = 0; sx < src_segment->w;  ++ sx) {
		uint32_t py = wrap((long)src_segment->top  + sy);
		uint32_t px = wrap((long)src_segment->left + sx);
		size_t pi = py * LITHOSPHERE_LEN + px;
		if (src->in_segment[pi] == src_si) {
			uint32_t wxy[2];
			plate_to_lithosphere(src, px, py, wxy);
			float mass = src->mass[pi];
			plate_set_mass(dst, wxy[0], wxy[1], 1, mass, dst_si);
			l->owner[wxy[1] * LITHOSPHERE_LEN + wxy[0]] = dst_pi;
			src->in_segment[pi] = NO_SEGMENT;
			src->mass[pi] = 0;
		}
	}
	/*
	 * Do not remove from vector because that would shuffle all our
	 * indices. Just zero area.
	 */
	src->segments[src_si].area = 0;
}

static uint32_t
segment_overlap(struct plate *p0, struct plate *p1, uint16_t si0, uint16_t si1)
{
	struct segment *s0 = p0->segments + si0;
	struct segment *s1 = p1->segments + si1;
	/* Segment overlapping bounding boxes in lithosphere space */
	uint32_t tl_p0[2];
	uint32_t tl_p1[2];
	uint32_t br_p0[2];
	uint32_t br_p1[2];
	plate_to_lithosphere(p0, s0->left, s0->top, tl_p0);
	plate_to_lithosphere(p1, s1->left, s1->top, tl_p1);
	plate_to_lithosphere(p0, s0->left + s0->w, s0->top + s0->h, br_p0);
	plate_to_lithosphere(p1, s1->left + s1->w, s1->top + s1->h, br_p1);
	uint32_t x0 = MIN(tl_p0[0], tl_p1[0]);
	uint32_t x1 = MIN(br_p0[0], br_p1[0]);
	uint32_t y0 = MIN(tl_p0[1], tl_p1[1]);
	uint32_t y1 = MIN(br_p0[1], br_p1[1]);
	uint32_t overlap = 0;
	for (uint32_t y = y0; y != y1; y = wrap((long)y+1))
	for (uint32_t x = x0; x != x1; x = wrap((long)x+1)) {
		uint32_t p0xy[2];
		uint32_t p1xy[2];
		lithosphere_to_plate(p0, x, y, p0xy);
		lithosphere_to_plate(p1, x, y, p1xy);
		size_t src0 = p0xy[1] * LITHOSPHERE_LEN + p0xy[0];
		size_t src1 = p1xy[1] * LITHOSPHERE_LEN + p1xy[0];
		if (p0->in_segment[src0] == si0 && p1->in_segment[src1] == si1)
			++ overlap;
	}
	return overlap;
}

static void
plate_blit(struct lithosphere *l, uint32_t pi,
           const struct tectonic_opts *opts)
{
	struct plate *p = l->plates + pi;
	for (uint32_t y = 0; y < LITHOSPHERE_LEN; ++ y)
	for (uint32_t x = 0; x < LITHOSPHERE_LEN; ++ x) {
		uint32_t pxy[2];
		lithosphere_to_plate(p, x, y, pxy);
		size_t dst = y * LITHOSPHERE_LEN + x;
		size_t src = pxy[1] * LITHOSPHERE_LEN + pxy[0];
		if (p->mass[src] <= 0)
			continue;
		if (l->owner[dst] == NO_PLATE) {
			/* No collision! Claim this cell */
			l->owner[dst] = pi;
			l->mass[dst] = p->mass[src];
			continue;
		}

		struct plate *prev_plate = l->plates + l->owner[dst];
		uint32_t pvxy[2];
		lithosphere_to_plate(prev_plate, x, y, pvxy);
		size_t prev_src = pvxy[1] * LITHOSPHERE_LEN + pvxy[0];

		/*
		 * Oceanic-oceanic subduction. This plate has less
		 * crust and thus subducts.
		 */
		if (p->mass[src] < TECTONIC_CONTINENT_MASS &&
		    l->mass[dst] > p->mass[src])
		{
			float xfer = MAX(MIN_XFER, opts->subduction_xfer *
			                           p->mass[src]);
			/* Remove transferred mass from source */
			p->mass[src] -= xfer;
			/* Add transferred mass to receiver */
			prev_plate->mass[prev_src] += xfer;
			l->mass[dst] += xfer;

			if (p->mass[src] <= 0) {
				p->in_segment[src] = NO_SEGMENT;
				continue;
			}
		}
		/*
		 * Oceanic-any subduction. The last plate is
		 * subducting. We don't treat an oceanic-oceanic
		 * collision any different from an oceanic-crust
		 * collision. We certainly could...
		 */
		else if (l->mass[dst] < TECTONIC_CONTINENT_MASS) {
			float xfer = MAX(MIN_XFER, opts->subduction_xfer *
			                           l->mass[dst]);
			/* Remove transferred mass from source */
			prev_plate->mass[prev_src] -= xfer;
			l->mass[dst] -= xfer;
			/* Add transferred mass to receiver */
			p->mass[src] += xfer;

			/* If last plate is gone, claim cell */
			if (l->mass[dst] <= 0) {
				prev_plate->in_segment[prev_src] = NO_SEGMENT;
				l->owner[dst] = pi;
				l->mass[dst] = p->mass[src];
				continue;
			}
		}
		/*
		 * Choosing which subducts within two plates creates some
		 * pretty horrid streaking, so this plate always subducts.
		 */
		else {
			float xfer = MAX(MIN_XFER, opts->collision_xfer *
			                           p->mass[src]);
			p->mass[src] -= xfer;
			prev_plate->mass[prev_src] += xfer;
			l->mass[dst] += xfer;

			if (p->mass[src] <= 0) {
				p->in_segment[src] = NO_SEGMENT;
				continue;
			}
		}
		/*
		 * Record a collision between the segment owning this cell and
		 * this segment.
		 */
		uint16_t siA = p->in_segment[src];
		uint16_t siB = prev_plate->in_segment[prev_src];
		if (siA != NO_SEGMENT && siB != NO_SEGMENT) {
			uint16_t si0 = siA < siB ? siA : siB;
			uint16_t si1 = siA < siB ? siB : siA;
			uint16_t cs = si0 * MAX_SEGMENT_COUNT + si1;
			unsigned char m = 1 << (cs % 8);
			if (l->collision_set[cs / 8] & m)
				continue;
			l->collision_set[cs / 8] |= m;
			vector_push(&l->collisions, (struct collision) {
				.plate_index = pi,
				.x = x, .y = y
			});
		}
	}
}

static void
plate_destroy(struct plate *p)
{
	vector_free(&p->segments);
}

static void
plate_growbfs_segment(struct plate *p, uint32_t **bfs, uint16_t si,
                      const struct tectonic_opts *opts)
{
	/*
	 * Funny how this ended up looking so much like our plate generation
	 * algorithm, except here we're not competing to create multiple fair
	 * plates.
	 */
	while (ring_size(*bfs)) {
		uint32_t source = *ring_head(*bfs);
		ring_pop(bfs);
		long srcx = source % LITHOSPHERE_LEN;
		long srcy = source / LITHOSPHERE_LEN;
		long r = opts->segment_radius;
		for (long yy = -r; yy <= r; ++ yy)
		for (long xx = -r; xx <= r; ++ xx) {
			uint32_t py = wrap(srcy + yy);
			uint32_t px = wrap(srcx + xx);
			uint32_t neighbor = py * LITHOSPHERE_LEN + px;
			if (p->in_segment[neighbor] == NO_SEGMENT &&
			    p->mass[neighbor] >= TECTONIC_CONTINENT_MASS)
			{
				/* claim and mark as new source */
				p->in_segment[neighbor] = si;
				ring_push(bfs, neighbor);
				/* grow segment area and dimensions */
				struct segment *s = p->segments + si;
				++ s->area;

				/* Restrict segment area to integer limit */
				if (s->area == MAX_SEGMENT_AREA)
					return;

				/* Maybe grow horizontally */
				if (px < s->left) {
					s->w += s->left - px;
					s->left = px;
				} else if (px - s->left + 1 > s->w) {
					s->w = px - s->left + 1;
				}
				/* Maybe grow vertically */
				if (py < s->top) {
					s->h += s->top - py;
					s->top = py;
				} else if (py - s->top + 1 > s->h) {
					s->h = py - s->top + 1;
				}

				/* Limit dimensions */
				if (s->w >= LITHOSPHERE_LEN) {
					s->left = 0;
					s->w = LITHOSPHERE_LEN;
				}
				if (s->h >= LITHOSPHERE_LEN) {
					s->top = 0;
					s->h = LITHOSPHERE_LEN;
				}
			}
		}
	}
}

static void
plate_init_velocity(struct plate *p, WELL512 rng)
{
	float theta = 6.28318530718f * WELL512f(rng);
	p->tx = 0;
	p->ty = 0;
	p->tvx = WELL512f(rng) * 2 * cosf(theta);
	p->tvy = WELL512f(rng) * 2 * cosf(theta);
}

static void
plate_segment(struct plate *p, uint16_t *sid_ter,
              const struct tectonic_opts *opts)
{
	vector_clear(&p->segments);
	memset(p->in_segment, 0xFF, LITHOSPHERE_AREA * sizeof(*p->in_segment));

	uint32_t *bfs = NULL;

	for (uint32_t y = 0; y < p->h; ++ y)
	for (uint32_t x = 0; x < p->w;  ++ x) {
		size_t i = wrap(p->top  + y) * LITHOSPHERE_LEN +
			   wrap(p->left + x);
		if (p->in_segment[i] != NO_SEGMENT ||
		    p->mass[i] < TECTONIC_CONTINENT_MASS)
		{
			continue;
		}
		/* Create segment */
		uint16_t si = vector_size(p->segments);
		assert(si < MAX_SEGMENT_COUNT);
		if (si == MAX_SEGMENT_COUNT)
			continue;
		vector_push(&p->segments, (struct segment) {
			.area = 1,
			.left = p->left + x,
			.top  = p->top  + y,
			.w    = 1,
			.h    = 1,
			.id   = ++ *sid_ter
		});
		/* Claim first cell */
		p->in_segment[i] = si;
		/* Perform breadth-first search */
		ring_push(&bfs, i);
		plate_growbfs_segment(p, &bfs, si, opts);
	}

	ring_free(&bfs);
}

static void
plate_set_mass(struct plate *p, uint32_t wx, uint32_t wy,
               int add_not_overwrite, float m, uint16_t si)
{
	uint32_t pxy[2];
	lithosphere_to_plate(p, wx, wy, pxy);
	size_t i = pxy[1] * LITHOSPHERE_LEN + pxy[0];
	/* Maybe grow horizontally */
	if (pxy[0] < p->left) {
		p->w += p->left - pxy[0];
		p->left = pxy[0];
	} else if (pxy[0] - p->left + 1 > p->w) {
		p->w = pxy[0] - p->left + 1;
	}
	/* Maybe grow vertically */
	if (pxy[1] < p->top) {
		p->h += p->top - pxy[1];
		p->top = pxy[1];
	} else if (pxy[1] - p->top + 1 > p->h) {
		p->h = pxy[1] - p->top + 1;
	}

	/* Limit dimensions */
	if (p->w >= LITHOSPHERE_LEN) {
		p->left = 0;
		p->w = LITHOSPHERE_LEN;
	}
	if (p->h >= LITHOSPHERE_LEN) {
		p->top = 0;
		p->h = LITHOSPHERE_LEN;
	}

	/* Add mass */
	if (add_not_overwrite)
		p->mass[i] += m;
	else
		p->mass[i] = m;
	if (p->in_segment[i] != si) {
		if (p->in_segment[i] != NO_SEGMENT)
			-- p->segments[p->in_segment[i]].area;
		if (si != NO_SEGMENT)
			++ p->segments[si].area;
		p->in_segment[i] = si;
	}
}
