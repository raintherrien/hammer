#include "hammer/cli.h"
#include "hammer/client/appstate.h"
#include "hammer/client/glthread.h"
#include "hammer/error.h"
#include <deadlock/dl.h>
#include <stdlib.h>

int
client_main(struct rtargs args)
{
	glthread_create();

	if (dlmainex(appstate_runner(), NULL, NULL, args.tc))
		xperror("Error creating deadlock scheduler");

	glthread_destroy();

	return EXIT_SUCCESS;
}
