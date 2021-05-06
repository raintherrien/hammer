#ifndef HAMMER_APPSTATE_WORLDGEN_H_
#define HAMMER_APPSTATE_WORLDGEN_H_

#include <deadlock/dl.h>

struct world_opts;

dltask *worldgen_appstate_alloc_detached(struct world_opts *);

#endif /* HAMMER_APPSTATE_WORLDGEN_H_ */
