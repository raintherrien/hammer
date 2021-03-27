#include "hammer/salloc.h"
#include "hammer/mem.h"
#include <assert.h>
#include <stdlib.h>

#define SALLOC_PAGESZ (1 << 16)

_Static_assert(SALLOC_PAGESZ > sizeof(max_align_t), "Stack allocator page size is not large enough to hold a max_align_t");
_Static_assert(SALLOC_PAGESZ % sizeof(max_align_t) == 0, "Stack allocator page size is not a multiple of max_align_t");

struct salloc_page {
	union {
		struct salloc_page *next;
		size_t allocated_bytes;
	};
	char buffer[SALLOC_PAGESZ];
};

/*
 * Attempt to take a free page from the pool. If no pages are available,
 * allocate and return one.
 */
static struct salloc_page *
salloc_take(struct salloc_pool *p)
{
	struct salloc_page *page;
	do {
		page = atomic_load_explicit(&p->free, memory_order_relaxed);
		if (page == NULL)
			return xmalloc(sizeof(*page));
	} while (!atomic_compare_exchange_weak(&p->free, &page, page->next));
	return page;
}

void *
salloc(struct salloc *fa, size_t nbytes)
{
	assert(nbytes < sizeof(max_align_t));
	if (fa->current == NULL ||
	    fa->current->allocated_bytes + nbytes >= SALLOC_PAGESZ)
	{
		struct salloc_page *next = salloc_take(fa->pool);
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
salloc_destroy(struct salloc_pool *pool)
{
	struct salloc_page *page = atomic_load_explicit(&pool->free, memory_order_relaxed);
	atomic_store_explicit(&pool->free, NULL, memory_order_relaxed);
	while (page) {
		struct salloc_page *this = page;
		page = page->next;
		free(this);
	}
}

void
salloc_init(struct salloc_pool *pool, size_t n, struct salloc *allocs)
{
	atomic_store_explicit(&pool->free, NULL, memory_order_relaxed);
	for (size_t i = 0; i < n; ++ i) {
		allocs[i] = (struct salloc) {
			.pool = pool,
			.allocated_list = NULL,
			.current = NULL
		};
	}
}

void
salloc_reset(size_t n, struct salloc *allocs)
{
	assert(n > 0);
	struct salloc_pool *pool = allocs[0].pool;
	struct salloc_page *free;
	struct salloc_page **tail = &free;
	for (size_t i = 0; i < n; ++ i) {
		struct salloc *fa = allocs + i;
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
		(*tail)->next = atomic_load_explicit(&pool->free, memory_order_relaxed);
	atomic_store_explicit(&pool->free, free, memory_order_relaxed);
}
