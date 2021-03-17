#include <stdio.h>

#define HMR_VERSION_MAJOR 0
#define HMR_VERSION_MINOR 1
#define HMR_VERSION_PATCH 0

void
print_help()
{
	puts("Usage: hammer [options]\n"
	     "Options:\n"
	     "  -h, --help     Print help and exit\n"
	     "      --size     Specify the world map size (default: 2048)\n"
	     "      --seed     Specify the world map seed (default randomly generated)\n"
	     "  -v, --version  Print version and exit\n");
}

void
print_version()
{
#define CHARVAL(C) (C - '0')

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
	       (year*10000+month*100+day));
}
