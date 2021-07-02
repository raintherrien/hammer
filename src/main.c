#include "hammer/cli.h"
#include "hammer/error.h"
#include <stdlib.h>

int client_main(struct rtargs);
int server_main(struct rtargs);

int
main(int argc, char **argv)
{
	/* Parse runtime args */
	struct rtargs args;
	switch (parse_args(&args, argc, argv)) {
	case 0:             break;
	case HAMMER_E_EXIT: return EXIT_SUCCESS;
	default:
		xperror("Error parsing command line arguments");
		return EXIT_FAILURE;
	}

	return args.server ? server_main(args) : client_main(args);
}
