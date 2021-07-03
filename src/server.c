#include "hammer/cli.h"
#include "hammer/error.h"
#include "hammer/net.h"
#include "hammer/server/appstate/transition_table.h"
#include <deadlock/dl.h>
#include <assert.h>

void
launch_local_server(void)
{
	dlasync(appstate_manager_init(&server_appstate_mgr, local_server_tt_sz, local_server_tt));
}

int
dedicated_server_main(struct rtargs args)
{
	assert(args.server);

	xpanic("Dedicated server unsupported");

	/*
	dltask *trampoline = appstate_manager_init(&server_appstate_mgr, dedicated_server_tt_sz, dedicated_server_tt);
	if (dlmainex(&server_task, NULL, NULL, args.tc))
		xperror("Error creating deadlock scheduler");
	*/

	return EXIT_SUCCESS;
}
