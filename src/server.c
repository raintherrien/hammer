#include "hammer/cli.h"
#include "hammer/error.h"
#include "hammer/mem.h"
#include "hammer/net.h"
#include <deadlock/dl.h>
#include <stdlib.h>

static void server_stub_async(DL_TASK_ARGS);

/*
 * External linkage in local_server.h, may be set up and invoked by a client
 * process to create a local server.
 */
dltask local_server_task;

void
local_server_init(void)
{
	local_server_task = DL_TASK_INIT(server_stub_async);
}

int
dedicated_server_main(struct rtargs args)
{
	xpanic("Dedicated server unsupported");

	local_server_init();
	if (dlmainex(&local_server_task, NULL, NULL, args.tc))
		xperror("Error creating deadlock scheduler");

	return EXIT_SUCCESS;
}

static void
server_stub_async(DL_TASK_ARGS)
{
	DL_TASK_ENTRY_VOID;

	server_peek(NULL, NULL);

	dltail(&local_server_task, server_stub_async);
}
