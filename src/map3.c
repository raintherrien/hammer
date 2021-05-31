#include "hammer/map3.h"
#include "hammer/mem.h"
#include <stdint.h>

#define MAP3_RESIZE_RATIO 0.9f
#define MAP3_INIT_SIZE 128
#define MAP3_NIL_HASH UINT64_MAX

struct map3_entry {
	void *data;
	uint64_t hash;
	size_t probe_length;
	int x, y, z;
};

static uint64_t map3_hash(int x, int y, int z);
static int      map3_eq(const struct map3_entry *, uint64_t, int, int, int);
static void     map3_resize(struct map3 *);
static void     map3_put_impl(struct map3 *, struct map3_entry);

static uint64_t
map3_hash(int x, int y, int z)
{
	/* For better or worse I'm using a 64-bit Fowler-Noll-Vo hash */
	const uint64_t prime = 1099511628211u;
	uint64_t hash  = 14695981039346656037u;
	const char *x_ = (const char *)&x;
	const char *y_ = (const char *)&y;
	const char *z_ = (const char *)&z;
	for (size_t i = 0; i < sizeof(int); ++ i) {
		hash ^= *x_ ++;
		hash *= prime;
		hash ^= *y_ ++;
		hash *= prime;
		hash ^= *z_ ++;
		hash *= prime;
	}
	return hash & MAP3_NIL_HASH;
}

static int
map3_eq(const struct map3_entry *e, uint64_t hash, int x, int y, int z)
{
	return e->hash == hash && e->x == x && e->y == y && e->z == z;
}

static void
map3_resize(struct map3 *m)
{
	size_t oldsize = m->entries_size;
	struct map3_entry *oldentries = m->entries;

	m->entries_size *= 2;
	m->entries = xcalloc(m->entries_size, sizeof(*m->entries));
	for (size_t i = 0; i < m->entries_size; ++ i) {
		m->entries[i].hash = MAP3_NIL_HASH;
		m->entries[i].probe_length = 0;
	}

	/* No need to rehash since we store unbounded hashes */
	for (size_t i = 0; i < oldsize; ++ i)
		map3_put_impl(m, oldentries[i]);

	free(oldentries);
}

static void
map3_put_impl(struct map3 *m, struct map3_entry e)
{
manual_tailcall: ;
	const size_t size_mask = m->entries_size - 1;
	const size_t index = e.hash & size_mask;
	size_t probe_length = 0;
	struct map3_entry *candidate = &m->entries[index];

	/*
	 * Note the ugly equality check we have to perform here and below,
	 * lest we turn this into a multi-map. TODO: Not a bad idea.
	 */
	while (candidate->probe_length > probe_length ||
	       ( candidate->probe_length == probe_length &&
	         !map3_eq(candidate, e.hash, e.x, e.y, e.z) ))
	{
		++ probe_length;
		candidate = &m->entries[(index + probe_length) & size_mask];
	}
	e.probe_length = probe_length;

	struct map3_entry richer = m->entries[(index + probe_length) & size_mask];
	m->entries[(index + probe_length) & size_mask] = e;
	if (richer.hash != MAP3_NIL_HASH &&
	    !map3_eq(&richer, e.hash, e.x, e.y, e.z))
	{
		e = richer;
		goto manual_tailcall;
	}
}

static void
map3_backshift(struct map3 *m, size_t index)
{
	const size_t size_mask = m->entries_size - 1;
	for (;;) {
		const size_t next = (index + 1) & size_mask;
		struct map3_entry *e = &m->entries[next];
		if (e->probe_length == 0)
			return;
		m->entries[index] = *e;
		e->hash = MAP3_NIL_HASH;
		index = next;
	}
}

void
map3_create(struct map3 *m)
{
	m->entries = xcalloc(MAP3_INIT_SIZE, sizeof(*m->entries));
	m->entries_size = MAP3_INIT_SIZE;
	m->entry_count = 0;
	for (size_t i = 0; i < MAP3_INIT_SIZE; ++ i) {
		m->entries[i].hash = MAP3_NIL_HASH;
		m->entries[i].probe_length = 0;
	}
}

void
map3_destroy(struct map3 *m)
{
	free(m->entries);
}

void
map3_del(struct map3 *m, int x, int y, int z)
{
	const size_t size_mask = m->entries_size - 1;
	const uint64_t hash = map3_hash(x, y, z);
	const size_t index = hash & size_mask;
	size_t probe_length = 0;
	struct map3_entry *e = &m->entries[index];
	/* Silently ignore attempts to delete non-existant */
	do {
		if (map3_eq(e, hash, x, y, z)) {
			-- m->entry_count;
			e->hash = MAP3_NIL_HASH;
			map3_backshift(m, (index + probe_length) & size_mask);
			return;
		}
		++ probe_length;
		e = &m->entries[(index + probe_length) & size_mask];
	} while (e->probe_length > 0);
}

void
map3_get(struct map3 *m, int x, int y, int z, void **d)
{
	const size_t size_mask = m->entries_size - 1;
	const uint64_t hash = map3_hash(x, y, z);
	const size_t index = hash & size_mask;
	size_t probe_length = 0;
	struct map3_entry *e = &m->entries[index];
	do {
		if (map3_eq(e, hash, x, y, z)) {
			*d = e->data;
			return;
		}
		++ probe_length;
		e = &m->entries[(index + probe_length) & size_mask];
	} while (probe_length <= e->probe_length);

	*d = NULL;
}

void
map3_put(struct map3 *m, int x, int y, int z, void *d)
{
	if (++ m->entry_count > MAP3_RESIZE_RATIO * m->entries_size)
		map3_resize(m);

	map3_put_impl(m, (struct map3_entry) {
		.data = d,
		.hash = map3_hash(x, y, z),
		.x = x, .y = y, .z = z
	});
}
