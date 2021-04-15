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
	struct main_menu_pkg main_menu;
	main_menu.task = DL_TASK_INIT(main_menu_entry);

	/* Parse runtime args */
	switch (parse_args(&main_menu.args, argc, argv)) {
	case 0:             break;
	case HAMMER_E_EXIT: return EXIT_SUCCESS;
	default:
		xpanic("Error parsing command line arguments");
	}

	glthread_create();

	if (dlmain(&main_menu.task, NULL, NULL))
		xpanic("Error creating deadlock scheduler");

	glthread_destroy();

	return EXIT_SUCCESS;
}
