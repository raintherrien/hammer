#ifndef HAMMER_SERVER_SERVER_H_
#define HAMMER_SERVER_SERVER_H_

#include "hammer/server_status.h"
#include "hammer/server/world.h"
#include "hammer/worldgen/planet.h"
#include <deadlock/dl.h>

struct server {
	enum server_status status;

	struct world world;

	struct planet_gen *planet_gen;

	int shutdown_requested;
	int is_shutdown;
};

void server_create(struct server *);
void server_destroy(struct server *);

#endif /* HAMMER_SERVER_SERVER_H_ */
