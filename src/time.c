#include "hammer/time.h"
#include <time.h>

time_ns
now_ns(void)
{
	struct timespec t;
#if _POSIX_C_SOURCE >= 199309L
	clock_gettime(CLOCK_REALTIME, &t);
#else
	timespec_get(&t, TIME_UTC);
#endif
	return t.tv_sec * 1000000000ull + t.tv_nsec;
}
