#ifndef HAMMER_MAP3_H_
#define HAMMER_MAP3_H_

#include <stddef.h>
#include <stdint.h>

#define MAP3_NIL_HASH UINT64_MAX

struct map3_entry {
	void *data;
	uint64_t hash;
	size_t probe_length;
	int x, y, z;
};

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

/*
 * map3_isvalid() may be used to iterate over map3::entries directly. For
 * example:
 *   for (size_t i = 0; i < map3->entries_size; ++ i)
 *     if (map3_isvalid(map3->entries[i]))
 *       process entry
 */
static inline int
map3_isvalid(const struct map3_entry *e) {
	return e->hash != MAP3_NIL_HASH;
}

#endif /* HAMMER_MAP3_H_ */
