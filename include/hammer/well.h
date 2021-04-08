#ifndef HAMMER_WELL_H_
#define HAMMER_WELL_H_

#include <stdint.h>

/*
 * Implementation of WELL512 by Chris Lomont, published in his paper:
 * Random Number Generation (2008) and released into the public domain.
 * http://lomont.org/papers/2008/Lomont_PRNG_2008.pdf
 */

#define WELL512SZ 16

typedef uint32_t WELL512[WELL512SZ+1]; /* +1 is additional index */

void WELL512_seed(WELL512, uint64_t seed);
uint32_t WELL512i(WELL512);
float WELL512f(WELL512);

#endif /* HAMMER_WELL_H_ */
