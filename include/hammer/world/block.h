#ifndef HAMMER_WORLD_BLOCK_H_
#define HAMMER_WORLD_BLOCK_H_

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
