#include "hammer/server/appstate/transition_table.h"
#include "hammer/server/server.h"
#include "hammer/mem.h"
#include "hammer/net.h"

static void appstate_start_local_async(DL_TASK_ARGS);

dltask *
appstate_start_local_enter(void *_)
{
	struct server *s = xmalloc(sizeof(*s));
	s->appstate_task = DL_TASK_INIT(appstate_start_local_async);

	/* Start the server requiring config */
	s->status = SERVER_STATUS_CONFIG;

	return &s->appstate_task;
}

void
appstate_start_local_exit(dltask *t)
{
	struct server *s = DL_TASK_DOWNCAST(t, struct server, appstate_task);

	/* If we were never configured, free the server */
	if (s->status == SERVER_STATUS_CONFIG) {
		free(s);
	}
}

static void
appstate_start_local_async(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct server, s, appstate_task);

	/* Respond to messages, wait for client to connect */
	enum netmsg_type type;
	size_t sz;
	while (server_peek(&type, &sz)) {
		switch (type) {
		case NETMSG_TYPE_HEARTBEAT_SYN:
			xpinfo("Responding to client heartbeat");
			server_discard(sz);
			server_write(NETMSG_TYPE_HEARTBEAT_ACK, NULL, 0);
			break;

		case NETMSG_TYPE_CLIENT_QUERY_SERVER_STATUS:
			xpinfo("Responding to client status query");
			server_discard(sz);
			server_write(NETMSG_TYPE_SERVER_RESPONSE_SERVER_STATUS, &s->status, sizeof(s->status));
			break;

		case NETMSG_TYPE_CLIENT_SET_SERVER_CONFIG:
			xpinfo("Accepting configuration from client");
			server_read(&s->world.opts, sizeof(s->world.opts));
			server_write(NETMSG_TYPE_SERVER_ACCEPTED_CONFIG, NULL, 0);
			break;

		default:
			errno = EINVAL;
			xperrorva("unexpected netmsg type: %d", type);
			server_discard(sz);
			break;
		}
	}

	dltail(&s->appstate_task, appstate_start_local_async);
}
