#ifndef HAMMER_WORLD_CHUNK_H_
#define HAMMER_WORLD_CHUNK_H_

#include "hammer/world/block.h"
#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#define CHUNK_LEN 32
#define CHUNK_VOL (CHUNK_LEN * CHUNK_LEN * CHUNK_LEN)
_Static_assert(CHUNK_VOL < (size_t)INT_MAX);

/*
 * A chunk is a multi-layer rhombus of blocks with a fixed size along each
 * four dimensions (q,r,s,y).
 *
 * Index axis in order of precedence:
 *   y is the layer, not an axial coordinate
 *   r is the row, each of which is shifted +w/2 forming the rhombus shape
 *   q is the cell within the row
 */
struct chunk {
	enum block blocks[CHUNK_VOL];
};

/*
 * Useful but trivial getter and setter. Note the axial coords.
 */

static inline enum block
chunk_block_at(const struct chunk *c, int y, int r, int q)
{
	return c->blocks[y * CHUNK_LEN * CHUNK_LEN + r * CHUNK_LEN + q];
}

static inline enum block *
chunk_block_at_ptr(struct chunk *c, int y, int r, int q)
{
	return &c->blocks[y * CHUNK_LEN * CHUNK_LEN + r * CHUNK_LEN + q];
}

#endif /* HAMMER_WORLD_CHUNK_H_ */
