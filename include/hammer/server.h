#ifndef HAMMER_SERVER_H_
#define HAMMER_SERVER_H_

#include "hammer/server/planet.h"
#include "hammer/server/world.h"

struct server {
	struct planet planet;
	struct world world;
};

extern struct server server;

#endif /* HAMMER_SERVER_H_ */
