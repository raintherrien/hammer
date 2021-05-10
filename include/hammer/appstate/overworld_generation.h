#ifndef HAMMER_APPSTATE_OVERWORLD_GENERATION_H_
#define HAMMER_APPSTATE_OVERWORLD_GENERATION_H_

#include <deadlock/dl.h>

struct world_opts;

dltask *overworld_generation_appstate_alloc_detached(struct world_opts *);

#endif /* HAMMER_APPSTATE_OVERWORLD_GENERATION_H_ */
