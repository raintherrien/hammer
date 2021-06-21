#include "hammer/chunkmgr.h"
#include <stdlib.h>

void chunkmgr_create(struct chunkmgr *mgr, const struct region *region)
{
	mgr->region = region;
	map3_create(&mgr->chunk_map);
	pool_create(&mgr->chunk_pool, sizeof(struct chunk));
}

void chunkmgr_destroy(struct chunkmgr *mgr)
{
	map3_destroy(&mgr->chunk_map);
	pool_destroy(&mgr->chunk_pool);
}

struct chunk *
chunkmgr_chunk_at(struct chunkmgr *mgr, int cy, int cr, int cq)
{
	void *d;
	map3_get(&mgr->chunk_map, (map3_key) { cy, cr, cq }, &d);
	return d;
}

struct chunk *
chunkmgr_create_at(struct chunkmgr *mgr, int cy, int cr, int cq)
{
	struct chunk *c = pool_take(&mgr->chunk_pool);
	/* XXX Populate chunk from region data */
	for (int y = 0; y < CHUNK_LEN; ++ y)
	for (int r = 0; r < CHUNK_LEN; ++ r)
	for (int q = 0; q < CHUNK_LEN; ++ q) {
		enum block b = rand() > RAND_MAX / 2 ? BLOCK_STONE : BLOCK_AIR;
		*chunk_block_at_ptr(c, y, r, q) = b;
	}
	map3_put(&mgr->chunk_map, (map3_key) { cy, cr, cq }, c);
	return c;
}
