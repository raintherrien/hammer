#include "hammer/cli.h"
#include "hammer/error.h"
#include "hammer/mem.h"
#include "hammer/worldgen/worldgen.h"
#include <deadlock/dl.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>

struct entry_exit_pkg {
	dltask task;
	struct rtargs rtargs;
	struct worldgen_pkg *worldgen;
};
static void entry_run(DL_TASK_ARGS);
static void exit_run(DL_TASK_ARGS);

static void worker_entry(int);

int
main(int argc, char **argv)
{
	struct entry_exit_pkg entry_exit;
	entry_exit.task = DL_TASK_INIT(entry_run);

	/* Parse runtime args */
	switch (parse_args(&entry_exit.rtargs, argc, argv)) {
	case 0:             break;
	case HAMMER_E_EXIT: return EXIT_SUCCESS;
	default:
		xperror("Error parsing command line arguments");
		return EXIT_FAILURE;
	}

	if (dlmain(&entry_exit.task, worker_entry, NULL)) {
		perror("Error in dlmain");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static void
entry_run(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct entry_exit_pkg, entry, task);
	entry->worldgen = xmalloc(sizeof(*entry->worldgen));
	worldgen_create(entry->worldgen, entry->rtargs.seed, entry->rtargs.size);
	dlcontinuation(&entry->task, exit_run);
	dlnext(&entry->worldgen->task, &entry->task);
	dlwait(&entry->task, 1);
	dlasync(&entry->worldgen->task);
}

static void
exit_run(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct entry_exit_pkg, exit, task);
	worldgen_destroy(exit->worldgen);
	free(exit->worldgen);
	puts("Goodbye!");
	dlterminate();
}

static void
worker_entry(int id)
{
	(void) id;
}
