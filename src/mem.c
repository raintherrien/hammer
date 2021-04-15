#include "hammer/mem.h"
#include "hammer/error.h"
#include <stdlib.h>

void *
xmalloc(size_t size)
{
	void *mem = malloc(size);
	if (mem == NULL)
		xpanicva("xmalloc failed to allocate %zu bytes", size);
	return mem;
}

void *
xcalloc(size_t nmemb, size_t size)
{
	void *mem = calloc(nmemb, size);
	if (mem == NULL)
		xpanicva("xcalloc failed to allocate %zu bytes", size);
	return mem;
}

void *
xrealloc(void *ptr, size_t size)
{
	void *mem = realloc(ptr, size);
	if (mem == NULL) {
		free(ptr); /* unnecessary */
		xpanicva("xrealloc failed to allocate %zu bytes", size);
	}
	return mem;
}
