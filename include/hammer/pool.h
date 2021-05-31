#ifndef HAMMER_POOL_H_
#define HAMMER_POOL_H_

#include <stddef.h>

#define POOL_PAGE_SIZE 512 /* 16MiB */

/*
 * freehead points to uninitialized data which can be cast to a pointer to the
 * next unitialized data. This singly linked list continues until all data are
 * utilized, then more is allocated. When data is returned it is added to this
 * free list.
 *
 * TOOD: This pool only ever grows, but it wouldn't be too hard to free a page
 * at a time when the entire page is free.
 */
struct pool {
	void **pages;
	void  *freehead;
	size_t structsize;
	size_t pages_count;
};

void  pool_create (struct pool *, size_t structsize);
void  pool_destroy(struct pool *);
void *pool_take   (struct pool *);
void  pool_give   (struct pool *, void *);

#endif /* HAMMER_POOL_H_ */
