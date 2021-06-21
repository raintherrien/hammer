#ifndef HAMMER_WORLD_BLOCK_H_
#define HAMMER_WORLD_BLOCK_H_

/*
 * Refer to hammer/hexagon.h for an explanation of pointy hexagon block
 * dimensions.
 */

#define BLOCK_HEX_SIZE   (1.0f) /* length of hexagon size */
#define BLOCK_EUC_HEIGHT (2.0f) /* dimensions of hexagon on Euclidean plane */
#define BLOCK_EUC_WIDTH  (1.73205f) /* sqrtf(3) * BLOCK_HEX_SIZE */

enum block {
	BLOCK_UNKNOWN = 0,
	BLOCK_AIR = 1,
	BLOCK_STONE = 2,
	BLOCK_DIRT = 3,
	BLOCK_SAND = 4,
	BLOCK_GRAVEL = 5,
	BLOCK_GRASS = 6,
	BLOCK_LOG1 = 7,
	BLOCK_LEAVES1 = 8,
	BLOCK_COUNT
};

static inline int
is_block_opaque(enum block b)
{
	return b != BLOCK_AIR;
}

#endif /* HAMMER_WORLD_BLOCK_H_ */
