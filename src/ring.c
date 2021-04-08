#include "hammer/ring.h"
#include "hammer/mem.h"
#include <stdlib.h>
#include <string.h>

void *
ring_grow_(void *r, size_t memb_size)
{
	/* Allocate the ring buffer */
	if (r == NULL) {
		size_t initial_capacity = 16; /* must be power of 2 */
		struct ring_sb *sb = xmalloc(sizeof(struct ring_sb) +
		                              initial_capacity * memb_size);
		sb->capacity = initial_capacity;
		sb->head = 0;
		sb->tail = 0;
		return ((char *)sb) + sizeof(struct ring_sb);
	}

	/* Is there space for one more entry? */
	struct ring_sb *sb = ring_sb_(r);
	if (sb->tail - sb->head < sb->capacity)
		return r;

	/* Grow geometrically */
	size_t old_capacity = sb->capacity;
	size_t new_capacity = sb->capacity * 2;
	struct ring_sb *new_sb = xmalloc(sizeof(struct ring_sb) + new_capacity * memb_size);
	void *new_r = (char *)new_sb + sizeof(struct ring_sb);
	new_sb->capacity = new_capacity;
	new_sb->head = 0;
	new_sb->tail = old_capacity;
	/* Copy head to capacity */
	size_t head_offset = sb->head & (old_capacity-1);
	memcpy((char *)new_r,
	       (char *)r + head_offset * memb_size,
	       (old_capacity - head_offset) * memb_size);
	/*
	 * If old data wrapped around end of buffer, memcpy to contiguous.
	 * Since we grow geometrically there's guaranteed to be room. If head
	 * is not at the starting position then we must have overflowed
	 * because we're resizing!
	 */
	if (head_offset != 0) {
		size_t shifting = (sb->tail & (old_capacity-1));
		memcpy((char *)new_r + (old_capacity - head_offset) * memb_size,
		       (char *)r, shifting * memb_size);
	}
	free(sb);
	return new_r;
}
