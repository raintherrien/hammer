#include "hammer/chunkmgr.h"

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
	map3_get(&mgr->chunk_map, cy, cr, cq, &d);
	return d;
}

struct chunk *
chunkmgr_create_at(struct chunkmgr *mgr, int cy, int cr, int cq)
{
	struct chunk *c = pool_take(&mgr->chunk_pool);
	/* XXX Populate chunk */
	map3_put(&mgr->chunk_map, cy, cr, cq, c);
	return c;
}
