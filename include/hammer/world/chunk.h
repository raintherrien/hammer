#ifndef HAMMER_WORLD_CHUNK_H_
#define HAMMER_WORLD_CHUNK_H_

#include "hammer/world/block.h"
#include <stddef.h>

#define CHUNK_LEN 32
#define CHUNK_VOL (CHUNK_LEN * CHUNK_LEN * CHUNK_LEN)

/*
 * A chunk is a multi-layer rhombus of blocks with a fixed size along each
 * four dimensions (q,r,s,y).
 *
 * Index axis in order of precedence:
 *   y is the layer, not an axial coordinate
 *   r is the row, each of which is shifted +w/2 forming the rhombus shape
 *   q is the cell within the row
 */
typedef block chunk[CHUNK_VOL];

/*
 * Useful but trivial getter and setter. Note the axial coords.
 */

static inline block
chunk_block_at(const chunk *c, size_t y, size_t r, size_t q)
{
	return c[y * CHUNK_LEN * CHUNK_LEN + r * CHUNK_LEN + q];
}

static inline block *
chunk_block_at_ptr(chunk *c, size_t y, size_t r, size_t q)
{
	return &c[y * CHUNK_LEN * CHUNK_LEN + r * CHUNK_LEN + q];
}

#endif /* HAMMER_WORLD_CHUNK_H_ */
