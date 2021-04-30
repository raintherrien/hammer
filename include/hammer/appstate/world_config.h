#ifndef HAMMER_APPSTATE_WORLD_CONFIG_H_
#define HAMMER_APPSTATE_WORLD_CONFIG_H_

#include <deadlock/dl.h>

struct rtargs;

dltask *world_config_appstate_alloc_detached(struct rtargs *);

#endif /* HAMMER_APPSTATE_WORLD_CONFIG_H_ */
