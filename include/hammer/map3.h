#ifndef HAMMER_MAP3_H_
#define HAMMER_MAP3_H_

#include <stddef.h>

struct map3_entry;

struct map3 {
	struct map3_entry *entries;
	size_t entries_size;
	size_t entry_count;
};

void map3_create(struct map3 *);
void map3_destroy(struct map3 *);

void map3_del(struct map3 *, int x, int y, int z);
void map3_get(struct map3 *, int x, int y, int z, void **);
void map3_put(struct map3 *, int x, int y, int z, void *);

#endif /* HAMMER_MAP3_H_ */
