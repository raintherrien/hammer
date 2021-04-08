#ifndef HAMMER_MEM_H_
#define HAMMER_MEM_H_

#include <stddef.h>

/*
 * malloc's sketchy half-brother. Prints to stderr and carelessly calls
 * abort if malloc returns NULL. At that point the world is probably
 * burning anyway so who cares.
 */
void *xmalloc(size_t size);
void *xcalloc(size_t nmemb, size_t size);
void *xrealloc(void *ptr, size_t size);

#endif /* HAMMER_MEM_H_ */
