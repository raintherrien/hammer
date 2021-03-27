#ifndef HAMMER_FALLOC_H_
#define HAMMER_FALLOC_H_

#include <stdatomic.h>
#include <stddef.h>

/*
 * TODO: Document fast multi-threaded frame allocator
 * TODO: must call falloc_reset before destroy
 */

struct falloc_page;

struct falloc_pool {
	_Atomic(struct falloc_page *) free;
};

struct falloc {
	struct falloc_pool *pool;
	struct falloc_page *allocated_list;
	struct falloc_page *current;
};

void *falloc        (struct falloc *, size_t nbytes);
void  falloc_destroy(struct falloc_pool *);
void  falloc_init   (struct falloc_pool *, size_t, struct falloc *);
void  falloc_reset  (size_t, struct falloc *);

#endif /* HAMMER_FALLOC_H_ */
