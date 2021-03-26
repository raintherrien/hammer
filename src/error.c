#include "hammer/error.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/*
 * ANSI escape codes to change our foreground text bold red and reset.
 */
#define ANSI_ESC_BOLDRED "\033[31;1m"
#define ANSI_ESC_RESET "\033[0m"

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

#undef ANSI_ESC_RESET
#undef ANSI_ESC_BOLDRED
