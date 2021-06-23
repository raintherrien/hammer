#include "hammer/chunkmgr.h"
#include "hammer/hexagon.h"
#include "hammer/math.h"
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
chunkmgr_chunk_at(struct chunkmgr *mgr, long cy, long cr, long cq)
{
	return map3_get(&mgr->chunk_map, (map3_key) { cy, cr, cq });
}

struct chunk *
chunkmgr_create_at(struct chunkmgr *mgr, long cy, long cr, long cq)
{
	struct chunk *c = pool_take(&mgr->chunk_pool);

	/* Populate chunk from region data */
	for (long y = 0; y < CHUNK_LEN; ++ y)
	for (long r = 0; r < CHUNK_LEN; ++ r)
	for (long q = 0; q < CHUNK_LEN; ++ q) {
		long yy = y + cy * CHUNK_LEN;
		long rr = r + cr * CHUNK_LEN;
		long qq = q + cq * CHUNK_LEN;

		enum block *b = chunk_block_at_ptr(c, y, r, q);

		/* Calculate position in region map */
		float fq = qq;
		float fr = rr;
		float x, z;
		hex_axial_to_pixel(1, fq, fr, &x, &z);

		/* Limit to region */
		if (x < 0 || z < 0 || x >= mgr->region->size || z >= mgr->region->size)
		{
			*b = BLOCK_AIR;
			continue;
		}

		*b = region_stone_at(mgr->region, x, z) < yy ? BLOCK_AIR : BLOCK_STONE;
	}
	map3_put(&mgr->chunk_map, (map3_key) { cy, cr, cq }, c);
	return c;
}

/*
 * TODO: Something like this function could identify max Y per chunk
void
chunkmgr_generate_all_debug(struct chunkmgr *mgr)
{
	const float s = mgr->region->size;
	float tl[2], tr[2], bl[2], br[2];
	hex_pixel_to_axial(1, 0, 0, tl+0, tl+1);
	hex_pixel_to_axial(1, s, 0, tr+0, tr+1);
	hex_pixel_to_axial(1, s, s, br+0, br+1);
	hex_pixel_to_axial(1, 0, s, bl+0, bl+1);
	float minr = floorf(MIN(tl[1], MIN(tr[1], MIN(bl[1], br[1]))) / CHUNK_LEN);
	float maxr = ceilf( MAX(tl[1], MAX(tr[1], MAX(bl[1], br[1]))) / CHUNK_LEN);
	float minq = floorf(MIN(tl[0], MIN(tr[0], MIN(bl[0], br[0]))) / CHUNK_LEN);
	float maxq = ceilf( MAX(tl[0], MAX(tr[0], MAX(bl[0], br[0]))) / CHUNK_LEN);

	for (long r = minr; r <= maxr; ++ r)
	for (long q = minq; q <= maxq; ++ q) {
		chunkmgr_create_at(mgr, 0, r, q);
	}
}
*/
