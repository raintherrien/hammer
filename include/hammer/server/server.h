#ifndef HAMMER_SERVER_SERVER_H_
#define HAMMER_SERVER_SERVER_H_

#include "hammer/server_status.h"
#include "hammer/server/world.h"
#include "hammer/worldgen/planet.h"
#include <deadlock/dl.h>

struct server {
	dltask appstate_task;

	enum server_status status;

	struct world world;

	struct planet_gen *planet_gen;
};

#endif /* HAMMER_SERVER_SERVER_H_ */
