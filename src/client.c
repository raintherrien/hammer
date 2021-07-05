#include "hammer/cli.h"
#include "hammer/client/appstate/transition_table.h"
#include "hammer/client/glthread.h"
#include "hammer/error.h"
#include <deadlock/dl.h>
#include <stdlib.h>

int
client_main(struct rtargs args)
{
	glthread_create();

	dltask *trampoline = appstate_manager_init(&client_appstate_mgr,
	                                           client_tt_sz, client_tt,
	                                           NULL);

	if (dlmainex(trampoline, NULL, NULL, args.tc))
		xperror("Error creating deadlock scheduler");

	glthread_destroy();

	return EXIT_SUCCESS;
}
