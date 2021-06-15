#ifndef HAMMER_WORLD_H_
#define HAMMER_WORLD_H_

#include "hammer/worldgen/region.h"
#include "hammer/worldgen/world_opts.h"

struct climate;
struct lithosphere;
struct stream_graph;

struct world {
	struct world_opts opts;

	/*
	 * These structs are only populated during planet and region
	 * generation due to their large size.
	 * XXX They belong in their own struct planet
	 */
	struct climate      *climate;
	struct lithosphere  *lithosphere;
	struct stream_graph *stream;

	struct region region;

	unsigned region_stream_coord_left;
	unsigned region_stream_coord_top;
	unsigned region_size_mag2;
};

extern struct world world;

#endif /* HAMMER_WORLD_H_ */
