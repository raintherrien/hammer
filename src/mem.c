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
