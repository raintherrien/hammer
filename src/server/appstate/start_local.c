#include "hammer/server/appstate/common.h"
#include "hammer/server/appstate/transition_table.h"
#include "hammer/server/server.h"
#include "hammer/mem.h"
#include "hammer/net.h"

static struct {
	dltask appstate_task;
	struct server *s;
} start_local;

static void appstate_start_local_async(DL_TASK_ARGS);

dltask *
appstate_start_local_enter(void *sx)
{
	start_local.s = sx;
	start_local.appstate_task = DL_TASK_INIT(appstate_start_local_async);
	/* Server requiring config */
	start_local.s->status = SERVER_STATUS_AWAITING_CONFIG;
	xpinfo("Server awaiting configuration");

	return &start_local.appstate_task;
}

void
appstate_start_local_exit(dltask *_)
{
	(void) _; /* Nothing to free */
}

static void
appstate_start_local_async(DL_TASK_ARGS)
{
	DL_TASK_ENTRY_VOID;

	/* Respond to messages, wait for client to connect */
	enum netmsg_type type;
	size_t sz;
	while (server_peek(&type, &sz)) {
		if (server_handle_common_netmsg(start_local.s, type, sz))
			continue;

		switch (type) {
		/*
		 * If the user sends us a config then kick off planet
		 * generation and acknowledge *after* the fact. We're passing
		 * planet generation our server instance but sending the
		 * acknowledgement is stateless.
		 */
		case NETMSG_TYPE_CLIENT_SET_SERVER_CONFIG:
			xpinfo("Accepting configuration from client");
			server_read(&start_local.s->world.opts, sizeof(start_local.s->world.opts));
			transition(&server_appstate_mgr, SERVER_APPSTATE_TRANSITION_PLANET_GENERATION, start_local.s);
			server_write(NETMSG_TYPE_SERVER_ACCEPTED_CONFIG, NULL, 0);
			break;

		default:
			errno = EINVAL;
			xperrorva("unexpected netmsg type: %d", type);
			server_discard(sz);
			break;
		}
	}
}
