#ifndef HAMMER_HEXAGON_H_
#define HAMMER_HEXAGON_H_

#include <math.h>

#define HEX_SQRT3 (1.73205080757f)

/*
 * Hexagonal Grids from Red Blob Games
 * https://www.redblobgames.com/grids/hexagons/

 * Copyright (c) 2021 Amit Patel
 * Distributed under the MIT license:
 * https://opensource.org/licenses/mit-license.php
 *
 * We use pointy hexagons 'round here. If you don't understand that, go read
 * Amit Patel's excellent article. For consistency we also use the axial
 * coordinates in said paper:
 *
 *      +q  -r  +s
 *        \  |  /
 *         \ | /
 *          \|/
 *          /|\
 *         / | \
 *        /  |  \
 *      -s  +r  -q
 *
 * If you're confused as to why r seems to be inverted relative to q/s, that's
 * okay. I was too. The best way to visualize this is to map a rhombus in
 * axial coordinates to a two-dimensional array.
 *
 * Chunks are stored as rhombus a (again, see the article) and are indexed by
 * the (q,r) dimensions:
 * (0,0) (1,0) (2,0) (3,0)
 *   (0,1) (1,1) (2,1) (3,1)
 *     (0,2) (1,2) (2,2) (3,2)
 *       (0,3) (1,3) (2,3) (3,3)
 * Notice that if you shift each row of hexagons by w/2 then (r,q) look an
 * awful lot like (x,z).
 *
 * When we're drawing a hexagon it looks like this:
 *
 * +--x
 * |       A
 * y  F         B
 *         +
 *    E         C
 *         D
 *
 * w = size * 2 * sin(60 degrees) = size * sqrt(3)
 * h = size * 2
 * m = size * 0.5
 * n = size * 1.5
 *
 * A = (w/2, 0)
 * B = (w  , m)
 * C = (w  , n)
 * D = (w/2, h)
 * E = (0  , n)
 * F = (0  , m)
 */

/*
 * I'm actually using Kenneth Shaw's implementation of Charles Chamber's code,
 * which can be found in the comments section of the article above
 */
static inline void
hex_pixel_to_axial(float hex_size,
                   float pixel_x, float pixel_z,
                   float *axial_q, float *axial_r)
{
	float hex_width = HEX_SQRT3 * hex_size;
	pixel_x = (pixel_x - hex_width / 2) / hex_width;
	float t1 = pixel_z / hex_size;
	float t2 = floorf(pixel_x + t1);
	*axial_r = floorf((floorf(t1 - pixel_x) + t2) / 3);
	*axial_q = floorf((floorf(2 * pixel_x + 1) + t2) / 3) - *axial_r;
}

static inline void
hex_axial_to_pixel(float hex_size,
                   float axial_q, float axial_r,
                   float *pixel_x, float *pixel_z)
{
	*pixel_x = hex_size * (HEX_SQRT3 * axial_q + HEX_SQRT3 / 2 * axial_r);
	*pixel_z = hex_size * (                               1.5f * axial_r);
}

#endif /* HAMMER_HEXAGON_H_ */
