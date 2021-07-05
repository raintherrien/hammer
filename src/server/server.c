#include "hammer/server/server.h"

void
server_create(struct server *s)
{
	s->status = SERVER_STATUS_UNKNOWN;
	s->shutdown_requested = 0;
	s->is_shutdown = 0;
}

void
server_destroy(struct server *s)
{
}
