#ifndef HAMMER_POISSON_H_
#define HAMMER_POISSON_H_

#include <stddef.h>
#include <stdint.h>

/*
 * Sets pt to an array of two dimensional points pt (and length ptsz)
 * distributed within a domain defined by the dimensions (w,h).
 *
 * This is a crude implementation of:
 *  Robert Bridson, 2007. "Fast Poisson Disk Sampling in Arbitrary
 *  Dimensions." SIGGRAPH '07
 */
void poisson(float **pt, size_t *ptsz, uint64_t seed, float r, float w, float h);

#endif /* HAMMER_POISSON_H_ */
