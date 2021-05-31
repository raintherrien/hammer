#ifndef HAMMER_CHUNKMGR_H_
#define HAMMER_CHUNKMGR_H_

#include "hammer/map3.h"
#include "hammer/pool.h"
#include "hammer/world/chunk.h"
#include "hammer/worldgen/region.h"
#include <stddef.h>

struct chunkmgr {
        const struct region *region;
        struct pool chunk_pool;
        struct map3 chunk_map;
};

static inline void
world_to_chunk(int wy, int wr, int wq, int *cy, int *cr, int *cq)
{
        *cy = wy / CHUNK_LEN;
        *cr = wr / CHUNK_LEN;
        *cq = wq / CHUNK_LEN;
}

void chunkmgr_create(struct chunkmgr *, const struct region *);
void chunkmgr_destroy(struct chunkmgr *);
struct chunk *chunkmgr_chunk_at(struct chunkmgr *, int cy, int cr, int cq);
struct chunk *chunkmgr_create_at(struct chunkmgr *, int cy, int cr, int cq);

#endif /* HAMMER_CHUNKMGR_H_ */
