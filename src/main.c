#include "hammer/cli.h"
#include "hammer/error.h"
#include <deadlock/dl.h>
#include <stdio.h>
#include <stdlib.h>

struct entry_pkg {
	dltask task;
	struct rtargs rtargs;
};
static void entry_run(DL_TASK_ARGS);

int
main(int argc, char **argv)
{
	struct entry_pkg entry;
	entry.task = DL_TASK_INIT(entry_run);

	/* Parse runtime args */
	switch (parse_args(&entry.rtargs, argc, argv)) {
	case 0:             break;
	case HAMMER_E_EXIT: return EXIT_SUCCESS;
	default:
		xperror("Error parsing command line arguments");
		return EXIT_FAILURE;
	}

	if (dlmain(&entry.task, NULL, NULL)) {
		perror("Error in dlmain");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static void
entry_run(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct entry_pkg, entry_ptr, task);
	dlterminate();
}
