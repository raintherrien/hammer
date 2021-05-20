#ifndef HAMMER_WORLDGEN_REGION_H_
#define HAMMER_WORLDGEN_REGION_H_

#include "hammer/neighborhood.h"
#include <stddef.h>

struct stream_graph;

/*
 * A region is an expanded section of the stream graph/climate map. Each
 * stream graph unit is interpolated between REGION_UPSCALE region cells along
 * each axis.
 */
#define REGION_UPSCALE 8
#define REGION_HEIGHT_SCALE 50

/*
 * Impose some sane limitations on the stream region size, which will be
 * multiplied by REGION_UPSCALE to determine region size.
 */
#define STREAM_REGION_SIZE_MIN 256
#define STREAM_REGION_SIZE_MAX 2048
#define STREAM_REGION_SIZE_MAX_MAG2 3 /* 256 * 2^3 = 2048 */

/*
 * Number of hydraulic erosion steps during region generation.
 * XXX: Erosion not implemented!
 */
#define REGION_GENERATIONS 1

/*
 * TODO: Explain region is a flat-topped hexagon, the 90 degree translation of
 * our smaller hexagons which are point-topped.
 *
 * TODO: This method of SPH hydraulic erosion could, I think pretty easily,
 * be applied to not only a region but also its Moore neighborhood, and
 * interpolation between regions generated separately could allow us to grow
 * the existing world post erosion with minimal artifacting. I have some idea
 * how this can work by blending new regions with existing "overscan" regions
 * based on the distance from the existing region. This requires generating
 * sequential regions upon a 2D lattice but that makes sense for this game.
 */
struct region {
        float *height;
        float *water;
        size_t hex_size;
        size_t rect_size;
        float  hex_height;
        float  hex_width;
        /*
         * Coordinates of the region within the stream graph (i.e. not taking
         * into account REGION_UPSCALE). Note that it's very likely the area
         * of our region will wrap around stream_graph->size.
         */
        unsigned stream_region_hex_size;
        unsigned stream_coord_left;
        unsigned stream_coord_top;
};

void region_create(struct region *,
                   unsigned stream_coord_left,
                   unsigned stream_coord_top,
                   unsigned stream_region_hex_size,
                   const struct stream_graph *);
void region_destroy(struct region *);
void region_erode(struct region *);

#endif /* HAMMER_WORLDGEN_REGION_H_ */
