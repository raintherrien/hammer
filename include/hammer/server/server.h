#ifndef HAMMER_SERVER_SERVER_H_
#define HAMMER_SERVER_SERVER_H_

#include "hammer/server_status.h"
#include "hammer/server/planet.h"
#include "hammer/server/world.h"
#include <deadlock/dl.h>

struct server {
	dltask appstate_task;

	enum server_status status;

	struct planet planet;
	struct world world;
};

#endif /* HAMMER_SERVER_SERVER_H_ */
