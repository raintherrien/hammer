#include "hammer/cli.h"
#include "hammer/error.h"
#include "hammer/game_states/main_menu.h"
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

	struct main_menu_pkg main_menu = main_menu_pkg_init(rtargs);
	if (dlmain(&main_menu.task, NULL, NULL))
		xpanic("Error creating deadlock scheduler");

	glthread_destroy();

	return EXIT_SUCCESS;
}
