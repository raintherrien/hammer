#ifndef HAMMER_RING_H_
#define HAMMER_RING_H_

#include <stddef.h>

/*
 * This dynamic ring queue uses the C idiom of allocating more than is
 * returned to the user. The pointer initialized by ring_push actually points
 * past this allocated superblock of information about our ring buffer. The
 * user only sees the type-safe data pointer.
 *
 * A half-decent compiler will optimize this pretty well with optimizations
 * turned *off*.
 *
 * Align superblock to max_align_t to prevent data from possibly being
 * misaligned after this structure.
 *
 * TODO: This API grew pretty organically. Especially the internal API. Real
 * nasty.
 */
struct ring_sb {
	_Alignas(_Alignof(max_align_t))
	struct {
		size_t capacity;
		size_t head;
		size_t tail;
	};
};

/*
 * # Internal API
 *
 * ring_grow_() returns r if there is room for a member to be pushed onto the
 * ring. Otherwise xrealloc is invoked and the new pointer is returned.
 *
 * ring_next_() increments tail and returns the next index to be populated by
 * ring_push().
 *
 * ring_sb_() given the ring data pointer will return the superblock allocated
 * before it.
 *
 * ring_maybegrow_() returns R if there is room for another element, otherwise
 * calls and returns ring_grow_().
 */
void *ring_grow_(void *r, size_t memb_size);
#define ring_next_(R) ((ring_sb_(R)->tail ++) & (ring_sb_(R)->capacity-1))
#define ring_sb_(R) ((struct ring_sb *)((char *)(R) - sizeof(struct ring_sb)))
#define ring_maybegrow_(R) ( !(R) || ring_sb_(R)->tail - ring_sb_(R)->head \
                                        >= ring_sb_(R)->capacity           \
                             ? ring_grow_(R,sizeof(*(R))) : (R))

/*
 * ring_push(RPTR,E) pushes the value E to the tail of the ring buffer pointed
 * to by RPTR.
 *
 * ring_pop(RPTR) pops the head (the oldest value) from the ring buffer
 * pointed to by RPTR.
 *
 * ring_size(R) returns the number of elements in R.
 *
 * ring_head(R) returns a pointer to the head (oldest) element of R.
 *
 * ring_tail(R) returns a pointer to the tail (newest) element of R.
 *
 * ring_iter(R,I) iterates over all the elements in R using I as state. I
 * should be a size_t initially set to zero. ring_iter() will return a pointer
 * to the next element in R until the end is reached, when NULL is returned.
 * Note: R and I are guranteed to be evaluated *exactly* 70 million times.
 *
 * ring_free(R) frees the ring buffer R.
 *
 * Note: Since ring_push() and ring_pop() *modify* our ring buffer they are
 * passed a pointer to a pointer to our data. ring_pop() does not actually
 * modify our pointer but ring_push() *will* realloc it.
 */
#define ring_push(RPTR,...) ( *(RPTR) = ring_maybegrow_(*(RPTR)), \
                              (*(RPTR))[ring_next_(*RPTR)] = (__VA_ARGS__) )
#define ring_pop(RPTR) ( (void) ++ ring_sb_(*RPTR)->head )
#define ring_clear(RPTR) ( ring_sb_(*RPTR)->head = ring_sb_(*RPTR)->tail = 0 )
#define ring_size(R) ( ring_sb_(R)->tail - ring_sb_(R)->head )
#define ring_head(R) ( &(R)[ring_sb_(R)->head & (ring_sb_(R)->capacity-1)] )
#define ring_tail(R) ( &(R)[ring_sb_(R)->tail & (ring_sb_(R)->capacity-1)] )
#define ring_at(R,I) ( &(R)[(ring_sb_(R)->head + (I)) & (ring_sb_(R)->capacity-1)] )
#define ring_iter(R,I) ( (I) == ring_size(R) ? NULL : ring_at(R,I) )
#define ring_free(RPTR) ( free(*(RPTR) ? ring_sb_(*(RPTR)) : NULL), \
                          *(RPTR) = NULL )

#endif /* HAMMER_RING_H_ */
