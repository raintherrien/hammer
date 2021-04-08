#include "hammer/well.h"
#include <stddef.h>

/*
 * Implementation of WELL512 by Chris Lomont, published in his paper:
 * Random Number Generation (2008) and released into the public domain.
 * http://lomont.org/papers/2008/Lomont_PRNG_2008.pdf
 */

static inline uint32_t
WELL512i_impl(uint32_t * restrict i, uint32_t s[restrict WELL512SZ])
{
	uint32_t a, b, c, d;
	a = s[*i];
	c = s[(*i + 13) & 15];
	b = a ^ c ^ (a << 16) ^ (c << 15);
	c = s[(*i + 9) & 15];
	c ^= (c >> 11);
	s[*i] = a = b ^ c;
	d = a ^ ((a << 5) & 0xDA442D24UL);
	*i = (*i + 15) & 15; /* set index */
	a = s[*i];
	s[*i] = a ^ b ^ d ^ (a << 2) ^ (b << 18) ^ (c << 28);
	return s[*i];
}

void
WELL512_seed(WELL512 well, uint64_t seed)
{
	well[0] = 0; /* Index */
	/* TODO: This is a pretty awful method of seeding; perhaps use LCG */
	for (size_t i = 0; i < WELL512SZ; ++ i)
		well[1+i] = seed;
}

uint32_t
WELL512i(WELL512 well)
{
	return WELL512i_impl(well, well+1);
}

float
WELL512f(WELL512 well)
{
	const float n = 1.0f / (float)((uint32_t)-1);
	return WELL512i_impl(well, well+1) * n;
}
