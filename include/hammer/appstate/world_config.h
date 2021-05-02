#ifndef HAMMER_APPSTATE_WORLD_CONFIG_H_
#define HAMMER_APPSTATE_WORLD_CONFIG_H_

#include <deadlock/dl.h>

struct world_opts {
	unsigned long long seed;
	unsigned long      size;
};

dltask *world_config_appstate_alloc_detached(void);

#endif /* HAMMER_APPSTATE_WORLD_CONFIG_H_ */
