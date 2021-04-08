#ifndef HAMMER_VECTOR_H_
#define HAMMER_VECTOR_H_

#include <stddef.h>

/*
 * TODO: Clean-up and document dynamic array API. This is pretty damn ugly and
 * poorly planned.
 */

struct vector_sb {
	_Alignas(_Alignof(max_align_t))
	struct {
		size_t capacity;
		size_t size;
	};
};

void *vector_grow_(void *v, size_t memb_size);
#define vector_sb_(V) ( (struct vector_sb *)((char *)(V) - sizeof(struct vector_sb)) )
#define vector_maybegrow_(V) ( !(V) || vector_sb_(V)->size          \
                                         == vector_sb_(V)->capacity \
                               ? vector_grow_(V,sizeof(*(V))) : (V))

#define vector_push(VPTR,...) ( *(VPTR) = vector_maybegrow_(*(VPTR)), \
                                (*(VPTR))[vector_sb_(*(VPTR))->size ++] = (__VA_ARGS__) )
#define vector_pushz(VPTR) ( *(VPTR) = vector_maybegrow_(*(VPTR)), \
                              memset(*(VPTR) + (vector_sb_(*(VPTR))->size ++), 0, sizeof(**(VPTR))) )
#define vector_pop(VPTR) ( -- vector_sb_(*(VPTR))->size )
#define vector_tail(V) ( &((V))[vector_sb_(V)->size - 1] )
#define vector_capacity(V) ( (V) ? vector_sb_(V)->capacity : 0 )
#define vector_size(V) ( (V) ? vector_sb_(V)->size : 0 )
#define vector_clear(VPTR) ( *(VPTR) ? vector_sb_(*(VPTR))->size = 0 : 0 )
#define vector_free(VPTR) ( free(*(VPTR) ? vector_sb_(*(VPTR)) : NULL), \
                            *(VPTR) = NULL )

#endif /* HAMMER_VECTOR_H_ */
