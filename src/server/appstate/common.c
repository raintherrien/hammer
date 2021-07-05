#include "hammer/error.h"
#include "hammer/server/appstate/common.h"
#include "hammer/server/server.h"

int
server_handle_common_netmsg(struct server *s, enum netmsg_type type, size_t sz)
{
	switch (type) {
	/* Always respond to heartbeats */
	case NETMSG_TYPE_HEARTBEAT_SYN:
		xpinfo("Responding to client heartbeat");
		server_discard(sz);
		server_write(NETMSG_TYPE_HEARTBEAT_ACK, NULL, 0);
		return 1;

	/* Always respond to status queries */
	case NETMSG_TYPE_CLIENT_QUERY_SERVER_STATUS:
		xpinfo("Responding to client status query");
		server_discard(sz);
		server_write(NETMSG_TYPE_SERVER_RESPONSE_SERVER_STATUS, &s->status, sizeof(s->status));
		return 1;

	/* Gracefully shutdown */
	case NETMSG_TYPE_CLIENT_REQUEST_SERVER_SHUTDOWN:
		xpinfo("Client has requested we shutdown");
		server_discard(sz);
		s->shutdown_requested = 1;
		return 1;

	default:
		return 0;
	}
}
