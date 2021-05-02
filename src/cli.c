#include "hammer/cli.h"

#include "hammer/error.h"
#include "hammer/version.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h> /* GetSystemInfo */
#else
#include <unistd.h> /* sysconf */
#endif

static void
print_help(void)
{
	printf("Usage: hammer [options]\n"
	       "Options:\n"
	       "  -h, --help     Print help and exit\n"
	       "      --tc       Specify the number of threads to spawn (default: numer of system threads)\n"
	       "  -v, --version  Print version and exit\n");
}

/*
 * Prints version information, my name, and GitHub link to stdio.
 */
static void
print_version(void)
{
	printf("hammer version %d.%d.%d %d by Rain Therrien\n"
	       "hammer is open source and freely distributable.\n"
	       "https://github.com/raintherrien/hammer\n",
	       HAMMER_VERSION_MAJOR, HAMMER_VERSION_MINOR, HAMMER_VERSION_PATCH,
	       build_date_code());
}

/* Return the number of system threads; TODO: Currently POSIX bound */
static inline unsigned int
system_threads(void)
{
#ifdef _WIN32
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	return sysinfo.dwNumberOfProcessors;
#else
	int rc = sysconf(_SC_NPROCESSORS_ONLN);
	if (rc == -1) {
		xperror("Error querying sysconf(_SC_NPROCESSORS_ONLN)");
		return 1;
	}
	return rc;
#endif
}

int
parse_args(struct rtargs *args, int argc, char **argv)
{
	/* Default values */
	args->tc   = system_threads();

	for (int i = 1; i < argc; ++ i) {
		if (!argv[i])
			continue;
		char *opt = argv[i];

		/* -h, --help */
		if (strcmp(opt, "-h") == 0 ||
		    strcmp(opt, "--help") == 0)
		{
			print_version();
			print_help();
			return HAMMER_E_EXIT;
		}

		/* --tc */
		else if (strcmp(opt, "--tc") == 0) {
			if (i == argc-1 || !argv[i+1]) {
				errno = EINVAL;
				xperror("--tc option not followed by value");
				return errno;
			}
			args->tc = strtoul(argv[i+1], NULL, 10);
			switch(args->tc) {
			case 0: errno = EINVAL; /* fall through */
			case ULONG_MAX:
				xperror("Invalid --tc value cannot be converted to unsigned long by strtoul");
				return errno;
			}
			++ i; /* Skip processing tc value */
		}

		/* -v, --version */
		else if (strcmp(opt, "-v") == 0 ||
		         strcmp(opt, "--version") == 0)
		{
			print_version();
			return HAMMER_E_EXIT;
		}

		/* Unknown, error */
		else {
			errno = EINVAL;
			xperrorva("Unknown command line argument #%d", i);
			return errno;
		}
	}

	return 0;
}
