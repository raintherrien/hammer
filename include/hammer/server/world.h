#ifndef HAMMER_SERVER_WORLD_H_
#define HAMMER_SERVER_WORLD_H_

#include "hammer/worldgen/region.h"
#include "hammer/worldgen/world_opts.h"

struct world {
	struct world_opts opts;
	struct region region;
	unsigned region_stream_coord_left;
	unsigned region_stream_coord_top;
	unsigned region_size_mag2;
};

#endif /* HAMMER_SERVER_WORLD_H_ */
