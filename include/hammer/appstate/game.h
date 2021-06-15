#ifndef HAMMER_APPSTATE_GAME_H_
#define HAMMER_APPSTATE_GAME_H_

#include <deadlock/dl.h>

struct region;

dltask *game_appstate_alloc_detached(const struct region *);

#endif /* HAMMER_APPSTATE_GAME_H_ */
