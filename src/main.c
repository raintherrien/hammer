#include "hammer/cli.h"
#include <deadlock/dl.h>
#include <stdio.h>
#include <stdlib.h>

struct entry_pkg { dltask task; };
static DL_TASK_DECL(entry_run);

int
main(int argc, char **argv)
{
	(void) argc;
	(void) argv;

	struct entry_pkg entry = {
		.task = DL_TASK_INIT(entry_run)
	};
	int rc = dlmain(&entry.task, NULL, NULL);
	if (rc)
		perror("Error in dlmain");
	return rc;
}

static DL_TASK_DECL(entry_run)
{
	DL_TASK_ENTRY(struct entry_pkg, entry_ptr, task);
	print_version();
	dlterminate();
}
