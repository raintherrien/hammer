#ifndef HAMMER_MATH_H_
#define HAMMER_MATH_H_

#include <assert.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265359f
#endif

#ifndef MIN
#define MIN(A,B) ((A) < (B) ? (A) : (B))
#endif

#ifndef MAX
#define MAX(A,B) ((A) > (B) ? (A) : (B))
#endif

#ifndef CLAMP
#define CLAMP(X,A,B) MIN(MAX(X,A),B)
#endif

#define GAUSSIANK(K,S) do {                        \
	float *k = (K);                            \
	size_t l = (S);                            \
	float h = (l - 1) / 2;                     \
	float sum = 0;                             \
	assert(l % 2 == 1);                        \
	for (size_t i = 0; i < l; ++ i) {          \
		float x = (i - h) / h;             \
		k[i] = expf(-x*x/2)*0.3989422804f; \
		sum += k[i];                       \
	}                                          \
	for (size_t i = 0; i < l; ++ i) {          \
		k[i] /= sum;                       \
	}                                          \
} while (0)

static inline unsigned long
wrapidx(long long index, unsigned long size)
{
	while (index < 0)
		index += size;
	return index % size;
}

#endif /* HAMMER_MATH_H_ */
