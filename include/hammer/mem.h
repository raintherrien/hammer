#ifndef HAMMER_MEM_H_
#define HAMMER_MEM_H_

#include "hammer/error.h"
#include <stddef.h>
#include <stdlib.h>

#define xmalloc(SIZE) xmalloc_impl(SIZE, __FILE__, __func__, __LINE__)
#define xcalloc(NMEMB, SIZE) xcalloc_impl(NMEMB, SIZE, __FILE__, __func__, __LINE__)
#define xrealloc(PTR, SIZE) xrealloc_impl(PTR, SIZE, __FILE__, __func__, __LINE__)

static inline void *
xmalloc_impl(size_t        size,
             const char   *file,
             const char   *function,
             unsigned long line)
{
	void *mem = malloc(size);
	if (mem == NULL) {
		xperrorva_impl(file, function, line,
		               "xmalloc failed to allocate %zu bytes", size);
		exit(errno);
	}
	return mem;
}

static inline void *
xcalloc_impl(size_t nmemb, size_t size,
             const char   *file,
             const char   *function,
             unsigned long line)
{
	void *mem = calloc(nmemb, size);
	if (mem == NULL) {
		xperrorva_impl(file, function, line,
		               "xcalloc failed to allocate %zu bytes", size);
		exit(errno);
	}
	return mem;
}

static inline void *
xrealloc_impl(void *ptr, size_t size,
              const char   *file,
              const char   *function,
              unsigned long line)
{
	void *mem = realloc(ptr, size);
	if (mem == NULL) {
		xperrorva_impl(file, function, line,
		               "xrealloc failed to allocate %zu bytes", size);
		exit(errno);
	}
	return mem;
}

#endif /* HAMMER_MEM_H_ */
