#include "hammer/mem.h"
#include "hammer/error.h"
#include <stdlib.h>

void *
xmalloc(size_t size)
{
	void *mem = malloc(size);
	if (mem == NULL) {
		xperrorva("xmalloc failed to allocate %zu bytes", size);
		abort();
	}
	return mem;
}

void *
xcalloc(size_t nmemb, size_t size)
{
	void *mem = calloc(nmemb, size);
	if (mem == NULL) {
		xperrorva("xcalloc failed to allocate %zu bytes", size);
		abort();
	}
	return mem;
}

void *
xrealloc(void *ptr, size_t size)
{
	void *mem = realloc(ptr, size);
	if (mem == NULL) {
		free(ptr); /* unnecessary */
		xperrorva("xrealloc failed to allocate %zu bytes", size);
		abort();
	}
	return mem;
}
