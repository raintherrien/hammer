#ifndef HAMMER_ERROR_H_
#define HAMMER_ERROR_H_

#include <errno.h>
#include <limits.h>
#include <stdlib.h> /* exit */

enum {
	HAMMER_E_EXIT = INT_MIN,
	HAMMER_E_MAXVAL, /* Used to construct error description table */
};

/*
 * ANSI escape codes to change our foreground text bold red and reset.
 */
#define ANSI_ESC_BOLDRED "\033[31;1m"
#define ANSI_ESC_RESET "\033[0m"

/*
 * xperror writes an error message to stderr, followed by the error
 * message associated with errno. Unlike perror, this is all prefixed
 * by the filename, line number, and function called from.
 *
 * xperrorva is similar except it invokes fprintf with the format string
 * and forwarded variadic arguments.
 */
#define xperror(MSG)        xperror_impl  (__FILE__, __func__, __LINE__, MSG)
#define xperrorva(FMT, ...) xperrorva_impl(__FILE__, __func__, __LINE__, FMT, __VA_ARGS__)
#define xpanic(MSG)   do { xperror(MSG);           exit(errno); } while (0)
#define xpanicva(...) do { xperrorva(__VA_ARGS__); exit(errno); } while (0)

#ifdef NDEBUG
#define xpinfo(MSG)
#define xpinfova(FMT, ...)
#else
#define xpinfo(MSG) xpinfo_impl  (__FILE__, __func__, __LINE__, MSG)
#define xpinfova(FMT, ...) xpinfova_impl(__FILE__, __func__, __LINE__, FMT, __VA_ARGS__)
#endif

/* Hey, if perror can ignore stderr write errors, so can we! */
void xperror_impl(
	const char   *file,
	const char   *function,
	unsigned long line,
	const char   *msg
);

void xperrorva_impl(
	const char   *file,
	const char   *function,
	unsigned long line,
	const char   *fmt,
	...
);

void xpinfo_impl(
	const char   *file,
	const char   *function,
	unsigned long line,
	const char   *msg
);

void xpinfova_impl(
	const char   *file,
	const char   *function,
	unsigned long line,
	const char   *fmt,
	...
);

#endif /* HAMMER_ERROR_H_ */
