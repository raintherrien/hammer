#include "hammer/cli.h"
#include "hammer/error.h"
#include "hammer/appstate/main_menu.h"
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
		xpanic("Error parsing command line arguments");
	}

	glthread_create();

	dltask *main_menu_task = main_menu_appstate_alloc_detached();
	if (dlmainex(main_menu_task, NULL, NULL, rtargs.tc))
		xpanic("Error creating deadlock scheduler");

	glthread_destroy();

	return EXIT_SUCCESS;
}
