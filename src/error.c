#include "hammer/error.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/*
 * These functions are real shitshows because it turns out getting the
 * string description of an errno in a multithreaded environment is
 * actually pretty annoying if you're trying to be standards compliant.
 * So I just abuse the fact you can pass perror a NULL argument and
 * it'll dump the description to stderr for us, in a thread-safe manner.
 */

void
xperror_impl(
	const char   *file,
	const char   *function,
	unsigned long line,
	const char   *msg
)
{
	fprintf(stderr, "%s:%lu:%s: " ANSI_ESC_BOLDRED, file, line, function);
	perror(msg);
	fputs(ANSI_ESC_RESET, stderr);
}


#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif
void
xperrorva_impl(
	const char   *file,
	const char   *function,
	unsigned long line,
	const char   *fmt,
	...
)
{
	va_list fwdargs;
	va_start(fwdargs, fmt);

	fprintf(stderr, "%s:%lu:%s: " ANSI_ESC_BOLDRED, file, line, function);
	vfprintf(stderr, fmt, fwdargs);
	fprintf(stderr, ": ");
	perror(NULL);
	fputs(ANSI_ESC_RESET, stderr);

	va_end(fwdargs);
}
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#undef ANSI_ESC_RESET
#undef ANSI_ESC_BOLDRED
