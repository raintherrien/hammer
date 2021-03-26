#ifndef HAMMER_CLI_H_
#define HAMMER_CLI_H_

#include <stddef.h>

/*
 * struct rtargs holds the value of arguments passed to our application
 * on the command line.
 *
 * parse_args() fills rtargs from argv or with sensible defaults.
 * Returns zero on success or sets and returns errno on error. May also
 * return HAMMER_E_EXIT if the program should exit gracefully e.g. when
 * the user passes the `--version` argument.
 *
 * parse_args() is also where we print program usage and version
 * information. We should probably change its name.
 */

struct rtargs {
	/*
	 * seed is used to seed (get it?) the pseudo-random world
	 * generation process, and whatever else needs semi-repeatable
	 * random elements.
	 */
	unsigned long long seed;

	/*
	 * size is the width and length of the world to generate.
	 */
	unsigned long size;

	/*
	 * tc specifies the thread count used by the deadlock scheduler.
	 * Useful for limiting hammer to only a few cores.
	 */
	unsigned long tc;
};

int parse_args(struct rtargs *, int argc, char **argv);

#endif /* HAMMER_CLI_H_ */
