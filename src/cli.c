#include "hammer/cli.h"

#include "hammer/error.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h> /* sysconf */

#define HMR_VERSION_MAJOR 0
#define HMR_VERSION_MINOR 1
#define HMR_VERSION_PATCH 0

#define CHARVAL(C) (C - '0')

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
	/* Expands to "??? ?? ????" if the date cannot be determined */
	const char *Mmm_dd_yyyy = __DATE__;
	int year  = 1992;
	int month = 4;
	int day   = 29;

	if (Mmm_dd_yyyy[0] != '?') {
		year = CHARVAL(Mmm_dd_yyyy[ 7])*1000 +
		       CHARVAL(Mmm_dd_yyyy[ 8])*100  +
		       CHARVAL(Mmm_dd_yyyy[ 9])*10   +
		       CHARVAL(Mmm_dd_yyyy[10]);
	}

	switch (Mmm_dd_yyyy[0]) {
	case 'A':
		if (Mmm_dd_yyyy[1] == 'p') month = 4;
		if (Mmm_dd_yyyy[1] == 'u') month = 8;
		break;
	case 'J':
		if (Mmm_dd_yyyy[1] == 'a') month = 1;
		if (Mmm_dd_yyyy[2] == 'n') month = 6;
		if (Mmm_dd_yyyy[2] == 'l') month = 7;
		break;
	case 'M':
		if (Mmm_dd_yyyy[2] == 'r') month = 3;
		if (Mmm_dd_yyyy[2] == 'y') month = 5;
		break;
	case 'D': month = 12; break;
	case 'F': month =  2; break;
	case 'N': month = 11; break;
	case 'O': month = 10; break;
	case 'S': month =  9; break;
	}

	if (Mmm_dd_yyyy[5] != '?') {
		day = CHARVAL(Mmm_dd_yyyy[5]);
		if (Mmm_dd_yyyy[4] != ' ') {
			day += CHARVAL(Mmm_dd_yyyy[4])*10;
		}
	}

	printf("hammer version %d.%d.%d %d by Rain Therrien\n"
	       "hammer is open source and freely distributable.\n"
	       "https://github.com/raintherrien/hammer\n",
	       HMR_VERSION_MAJOR, HMR_VERSION_MINOR, HMR_VERSION_PATCH,
	       year*10000+month*100+day);
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
	return time(NULL);
}

/* Return the number of system threads; TODO: Currently POSIX bound */
static inline unsigned int
system_threads(void)
{
	int rc = sysconf(_SC_NPROCESSORS_ONLN);
	if (rc == -1) {
		xperror("Error querying sysconf(_SC_NPROCESSORS_ONLN)");
		return 1;
	}
	return rc;
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
