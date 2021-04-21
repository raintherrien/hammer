#include "hammer/cli.h"
#include "hammer/error.h"
#include "hammer/appstate/main_menu.h"
#include "hammer/appstate/manager.h"
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

	struct appstate_manager appmgr;
	appstate_manager_create(&appmgr, appstate_main_menu_alloc(rtargs));

	if (dlmain(&appmgr.task, NULL, NULL))
		xpanic("Error creating deadlock scheduler");

	appstate_manager_destroy(&appmgr);

	glthread_destroy();

	return EXIT_SUCCESS;
}
