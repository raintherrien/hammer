#ifndef HAMMER_APPSTATE_REGION_GENERATION_H_
#define HAMMER_APPSTATE_REGION_GENERATION_H_

#include <deadlock/dl.h>

struct climate;
struct lithosphere;
struct stream_graph;
struct world_opts;

dltask *region_generation_appstate_alloc_detached(const struct climate *,
                                                  const struct lithosphere *,
                                                  const struct stream_graph *,
                                                  const struct world_opts *,
                                                  unsigned stream_coord_left,
                                                  unsigned stream_coord_top,
                                                  unsigned stream_region_size);

#endif /* HAMMER_APPSTATE_REGION_GENERATION_H_ */
