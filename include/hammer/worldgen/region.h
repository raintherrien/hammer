#ifndef HAMMER_WORLDGEN_REGION_H_
#define HAMMER_WORLDGEN_REGION_H_

#include "hammer/neighborhood.h"
#include <stddef.h>

struct stream_graph;

/* A region is an expanded section of 512x512 composite map */
#define OVERWORLD_REGION_LEN 512
#define REGION_LEN 2048
#define REGION_AREA (REGION_LEN * REGION_LEN)

/*
 * Hydraulic erosion is performed upon a region and its Moore neighborhood to
 * prevent boundary artifacts and to facilitate interpolation between regions
 * generated separately. TODO: I have some idea how this can work by blending
 * new regions with existing overscan regions, based on the distance from the
 * existing region. This requires generating sequential regions upon a 2D
 * lattice but that makes sense for this game.
 */
struct region_nh {
	float height[MOORE_NEIGHBORHOOD_SIZE][REGION_AREA];
	size_t central_index;
};

void region_nh_create(struct region_nh *,
                      unsigned x, unsigned y,
                      const struct stream_graph *);
void region_nh_destroy(struct region_nh *);
void region_nh_erode(struct region_nh *);

/* TODO: void region_nh_extend_lerp(struct region_nh * restrict new
                                    struct region_nh * restrict old); */

#endif /* HAMMER_WORLDGEN_REGION_H_ */
