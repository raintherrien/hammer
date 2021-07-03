#include "hammer/cli.h"
#include "hammer/error.h"
#include "hammer/mem.h"
#include "hammer/net.h"
#include <deadlock/dl.h>
#include <stdlib.h>
#include <stdio.h> /* debugging */

static void server_stub_async(DL_TASK_ARGS);

/*
 * External linkage in local_server.h, may be set up and invoked by a client
 * process to create a local server.
 */
dltask server_task;

void
launch_local_server(void)
{
	server_task = DL_TASK_INIT(server_stub_async);
	dlasync(&server_task);
}

int
dedicated_server_main(struct rtargs args)
{
	xpanic("Dedicated server unsupported");

	server_task = DL_TASK_INIT(server_stub_async);
	/* server_connect_network() */
	if (dlmainex(&server_task, NULL, NULL, args.tc))
		xperror("Error creating deadlock scheduler");

	return EXIT_SUCCESS;
}

static void
server_stub_async(DL_TASK_ARGS)
{
	DL_TASK_ENTRY_VOID;

	/* Respond to any messages */
	enum netmsg_type type;
	size_t sz;
	while (server_peek(&type, &sz)) {
		switch (type) {
		case NETMSG_TYPE_HEARTBEAT_SYN:
			printf("client requested heartbeat\n");
			server_discard(sz);
			server_write(NETMSG_TYPE_HEARTBEAT_ACK, NULL, 0);
			break;
		default:
			errno = EINVAL;
			xperrorva("unexpected netmsg type: %d", type);
			server_discard(sz);
			break;
		}
	}

	dltail(&server_task, server_stub_async);
}
