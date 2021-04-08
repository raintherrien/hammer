#ifndef HAMMER_WORLDGEN_WORLDGEN_H_
#define HAMMER_WORLDGEN_WORLDGEN_H_

#include "hammer/worldgen/large_scale_world_gen.h"
#include <deadlock/dl.h>

struct worldgen_pkg {
	dltask task;
	struct large_scale_world_gen_pkg large_scale_world_gen_pkg;
	unsigned int iteration;
};

void worldgen_create(struct worldgen_pkg *, unsigned long long seed, unsigned long size);
void worldgen_destroy(struct worldgen_pkg *);

#endif /* HAMMER_WORLDGEN_WORLDGEN_H_ */
