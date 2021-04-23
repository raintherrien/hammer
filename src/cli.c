#include "hammer/cli.h"

#include "hammer/error.h"
#include "hammer/version.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
	       "      --size     Specify the world map size (default: 2048)\n"
	       "      --seed     Specify the world map seed (default: randomly generated)\n"
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

/*
 * Try to read /dev/urandom, fall back to calling time.
 */
static inline unsigned long long
random_seed(void)
{
	FILE *dr = fopen("/dev/urandom", "r");
	if (dr) {
		unsigned long long rand;
		size_t nread = fread(&rand, sizeof(rand), 1, dr);
		if (fclose(dr))
			xperror("Error closing /dev/random FILE");
		if (nread == 1)
			return rand;
	}
	return (unsigned long long)time(NULL);
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
	args->size = 2048;
	args->seed = random_seed();
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

		/* --size */
		else if (strcmp(opt, "--size") == 0) {
			if (i == argc-1 || !argv[i+1]) {
				errno = EINVAL;
				xperror("--size option not followed by value");
				return errno;
			}
			args->size = strtoul(argv[i+1], NULL, 10);
			switch(args->size) {
			case 0: errno = EINVAL; /* fall through */
			case ULONG_MAX:
				xperror("Invalid --size value cannot be converted to unsigned long by strtoul");
				return errno;
			}
			++ i; /* Skip processing size value */
		}

		/* --seed */
		else if (strcmp(opt, "--seed") == 0) {
			if (i == argc-1 || !argv[i+1]) {
				errno = EINVAL;
				xperror("--seed option not followed by value");
				return errno;
			}
			args->seed = strtoull(argv[i+1], NULL, 10);
			switch(args->seed) {
			case 0: errno = EINVAL; /* fall through */
			case ULONG_MAX:
				xperror("Invalid --seed value cannot be converted to unsigned long long by strtoull");
				return errno;
			}
			++ i; /* Skip processing seed value */
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
