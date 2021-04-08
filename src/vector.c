#include "hammer/vector.h"
#include "hammer/mem.h"
#include <stdlib.h>

void *
vector_grow_(void *v, size_t memb_size)
{
	if (v == NULL) {
		void *mem = xmalloc(sizeof(struct vector_sb) + memb_size);
		struct vector_sb *sb = mem;
		sb->capacity = 1;
		sb->size = 0;
		return ((char *)mem) + sizeof(struct vector_sb);
	}
	struct vector_sb *sb = vector_sb_(v);
	if (sb->size == sb->capacity) {
		sb->capacity *= 2;
		void *mem = xrealloc(sb, sizeof(struct vector_sb) + sb->capacity * memb_size);
		return ((char *)mem) + sizeof(struct vector_sb);
	}
	return v;
}
