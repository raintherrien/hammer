#include "hammer/pool.h"
#include "hammer/mem.h"

static int
pool_grow(struct pool *p)
{
	size_t np = p->pages_count ++;
	p->pages = xrealloc(p->pages, p->pages_count * sizeof(*p->pages));
	p->pages[np] = xcalloc(POOL_PAGE_SIZE, p->structsize);
	/* Freelist should be empty. Fill with this page. */
	p->freehead = p->pages[np];
	void **next = (void **)p->freehead;
	for (size_t i = 1; i+1 < POOL_PAGE_SIZE; ++ i) {
		*next = (char *)p->pages[np] + i*p->structsize;
		next = (void **)*next;
	}
	*next = NULL;
}

void
pool_create(struct pool *p, size_t structsize)
{
	/* Ensure we can alias our linked list */
	if (structsize < sizeof(void *))
		structsize = sizeof(void *);

	/* Allocate initial page */
	p->pages = NULL;
	p->structsize = structsize;
	p->pages_count = 0;
	pool_grow(p);
}

void
pool_destroy(struct pool *p)
{
	free(p->pages);
}

void *
pool_take(struct pool *p)
{
	if (p->freehead == NULL)
		pool_grow(p);

	/* Pop the top from our free list */
	void *taken = p->freehead;
	p->freehead = *(void **)p->freehead;
	return taken;
}

void
pool_give(struct pool *p, void *data)
{
	/*
	 * The returned memory becomes the head of our free list. This way we
	 * don't need to traverse our list or check whether freehead is NULL.
	 */
	*(void **)data = p->freehead;
	p->freehead = data;
}
