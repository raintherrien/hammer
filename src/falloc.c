#include "hammer/falloc.h"
#include "hammer/mem.h"
#include <assert.h>
#include <stdlib.h>

#define FALLOC_PAGESZ (1 << 16)

_Static_assert(FALLOC_PAGESZ > sizeof(max_align_t), "Frame allocator page size is not large enough to hold a max_align_t");
_Static_assert(FALLOC_PAGESZ % sizeof(max_align_t) == 0, "Frame allocator page size is not a multiple of max_align_t");

struct falloc_page {
	union {
		struct falloc_page *next;
		size_t allocated_bytes;
	};
	char buffer[FALLOC_PAGESZ];
};

/*
 * Attempt to take a free page from the pool. If no pages are available,
 * allocate and return one.
 */
static struct falloc_page *
falloc_take(struct falloc_pool *p)
{
	struct falloc_page *page;
	do {
		page = atomic_load_explicit(&p->free, memory_order_relaxed);
		if (page == NULL)
			return xmalloc(sizeof(*page));
	} while (!atomic_compare_exchange_weak(&p->free, &page, page->next));
	return page;
}

void *
falloc(struct falloc *fa, size_t nbytes)
{
	assert(_Alignof(nbytes) < sizeof(max_align_t));
	if (fa->current == NULL ||
	    fa->current->allocated_bytes + nbytes >= FALLOC_PAGESZ)
	{
		struct falloc_page *next = falloc_take(fa->pool);
		next->allocated_bytes = 0;
		/*
		 * If this is our first allocation, begin our allocated
		 * linked list. Otherwise add this new page to the end
		 * of that list.
		 */
		if (fa->current)
			fa->current->next = next;
		else
			fa->allocated_list = next;
		fa->current = next;
	}
	void *ptr = fa->current->buffer + fa->current->allocated_bytes;
	fa->current->allocated_bytes += nbytes;
	return ptr;
}

void
falloc_destroy(struct falloc_pool *pool)
{
	struct falloc_page *page = pool->free;
	pool->free = NULL;
	while (page) {
		struct falloc_page *this = page;
		page = page->next;
		free(this);
	}
}

void
falloc_init(struct falloc_pool *pool, size_t n, struct falloc *allocs)
{
	pool->free = NULL;
	for (size_t i = 0; i < n; ++ i) {
		allocs[i] = (struct falloc) {
			.pool = pool,
			.allocated_list = NULL,
			.current = NULL
		};
	}
}

void
falloc_reset(size_t n, struct falloc *allocs)
{
	assert(n > 0);
	struct falloc_pool *pool = allocs[0].pool;
	struct falloc_page *free;
	struct falloc_page **tail = &free;
	for (size_t i = 0; i < n; ++ i) {
		struct falloc *fa = allocs + i;
		assert(fa->pool == pool);
		if (!fa->current)
			continue;
		/* Set next on current union */
		fa->current->next = NULL;
		/* Combine lists */
		*tail = fa->allocated_list;
		/* Find the end of our free list */
		while (*tail)
			tail = &(*tail)->next;
		/* Reset allocator */
		fa->allocated_list = NULL;
		fa->current = NULL;
	}
	if (*tail)
		(*tail)->next = pool->free;
	pool->free = free;
}
