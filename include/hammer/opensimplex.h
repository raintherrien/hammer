#ifndef HAMMER_OPENSIMPLEX_H_
#define HAMMER_OPENSIMPLEX_H_

/*
 * Original OpenSimplex Noise in Java.
 * by Kurt Spencer
 *
 * v1.1 (October 5, 2014)
 * - Added 2D and 4D implementations.
 * - Proper gradient sets for all dimensions, from a
 *   dimensionally-generalizable scheme with an actual
 *   rhyme and reason behind it.
 * - Removed default permutation array in favor of
 *   default seed.
 * - Changed seed-based constructor to be independent
 *   of any particular randomization library, so results
 *   will be the same when ported to other languages.
 *
 * Poorly translated to C
 */

struct opensimplex;

struct opensimplex *opensimplex_alloc(unsigned long long seed);
void opensimplex_free(struct opensimplex *);

float opensimplex2(const struct opensimplex *, float, float);
float opensimplex3(const struct opensimplex *, float, float, float);
float opensimplex4(const struct opensimplex *, float, float, float, float);
float opensimplex2_fbm(const struct opensimplex *, float, float, int oct, float per);
float opensimplex3_fbm(const struct opensimplex *, float, float, float, int oct, float per);
float opensimplex4_fbm(const struct opensimplex *, float, float, float, float, int oct, float per);

#endif /* HAMMER_OPENSIMPLEX_H_ */
