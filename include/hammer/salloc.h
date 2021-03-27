#ifndef HAMMER_SALLOC_H_
#define HAMMER_SALLOC_H_

#include <stdatomic.h>
#include <stddef.h>

/*
 * TODO: Document fast multi-threaded stack allocator
 * TODO: must call salloc_reset before destroy
 */

struct salloc_page;

struct salloc_pool {
	_Atomic(struct salloc_page *) free;
};

struct salloc {
	struct salloc_pool *pool;
	struct salloc_page *allocated_list;
	struct salloc_page *current;
};

void *salloc        (struct salloc *, size_t nbytes);
void  salloc_destroy(struct salloc_pool *);
void  salloc_init   (struct salloc_pool *, size_t, struct salloc *);
void  salloc_reset  (size_t, struct salloc *);

#endif /* HAMMER_SALLOC_H_ */
