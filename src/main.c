#include "hammer/cli.h"
#include "hammer/error.h"
#include "hammer/appstate.h"
#include "hammer/glthread.h"
#include <deadlock/dl.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char **argv)
{
	/* Parse runtime args */
	struct rtargs rtargs;
	switch (parse_args(&rtargs, argc, argv)) {
	case 0:             break;
	case HAMMER_E_EXIT: return EXIT_SUCCESS;
	default:
		xperror("Error parsing command line arguments");
		return EXIT_FAILURE;
	}

	glthread_create();

	if (dlmainex(appstate_runner(), NULL, NULL, rtargs.tc))
		xperror("Error creating deadlock scheduler");

	glthread_destroy();

	return EXIT_SUCCESS;
}
