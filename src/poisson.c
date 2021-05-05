#include "hammer/math.h"
#include "hammer/mem.h"
#include "hammer/well.h"
#include <stdlib.h>
#include <string.h>

/*
 * Sets pt to an array of two dimensional points pt (and length ptsz)
 * distributed within a domain defined by the dimensions (w,h). Returns zero
 * on success or sets and returns errno on error. pt and ptsz are unspecified
 * on error.
 *
 * This is a crude implementation of:
 *  Robert Bridson, 2007. "Fast Poisson Disk Sampling in Arbitrary
 *  Dimensions." SIGGRAPH '07
 */

void
poisson(float **pt, size_t *ptsz, uint64_t seed, float r, float w, float h)
{
	WELL512 rng;
	WELL512_seed(rng, seed);

	/*
	 * Backing grid to accelerate spacial searches stores the index into
	 * (*pt) of the point at that location (position floored).
	 *
	 * Backing grid cells have length radius/sqrt(2) such that two points
	 * farther than radius apart will never occupy the same cell.
	 */
	float cl = r / 1.414213562f;
	long gw = lroundf(ceilf(w / cl));
	long gh = lroundf(ceilf(h / cl));

	/* Buffer for points, with reasonable maximum count of area */
	size_t mxnpt = gw * gh;
	*pt = xmalloc(2 * mxnpt * sizeof **pt);
	size_t npt = 0;

	/* Index into pt, queried with grid coordinate */
	size_t *lookup = xmalloc(gw * gh * sizeof *lookup);

	/* Active list of points that have not yet spawned children */
	size_t *active = xmalloc(gw * gh * sizeof *active);
	size_t nactive = 0;

	/* Assign initial values to SIZE_MAX, which means no point */
	memset(lookup, 0xFF, gw * gh * sizeof *lookup);

	/* Prime with one random point */
	{
		float strtx = WELL512f(rng) * w;
		float strty = WELL512f(rng) * h;
		long gridx = lroundf(floorf(strtx / cl));
		long gridy = lroundf(floorf(strty / cl));
		(*pt)[0] = strtx;
		(*pt)[1] = strty;
		lookup[gridy * gw + gridx] = 0;
		active[0] = 0;
		npt = 1;
		nactive = 1;
	}

	do {
		float *parent = (*pt) + 2*active[-- nactive];

		/* Maximum attempts made to spawn a child point */
		size_t max_attempts = 8;
		for (size_t i = 0; i < max_attempts && npt < mxnpt; ++ i) {
			/* Calculate a new point between r and 2*r */
			float dist = r * (1 + WELL512f(rng));
			float angle = 6.28318530717958647692f * WELL512f(rng);
			float childx = parent[0] + cosf(angle) * dist;
			float childy = parent[1] + sinf(angle) * dist;

			/* Wrap child coords to domain */
			while (childx < 0)
				childx += w;
			while (childy < 0)
				childy += h;
			childx = fmodf(childx, w);
			childy = fmodf(childy, h);

			long gridx = lroundf(floorf(childx / cl));
			long gridy = lroundf(floorf(childy / cl));

			int gridi = gridy * gw + gridx;
			int intersect = 0;
			/* TODO: Doesn't need to check corner neighbors */
			for (int ny = -2; ny <= 2 && !intersect; ++ ny)
			for (int nx = -2; nx <= 2 && !intersect; ++ nx) {
				size_t ni = lookup[wrapidx(gridy+ny,gh) * gw +
				                   wrapidx(gridx+nx,gw)];
				if (ni == SIZE_MAX)
					continue;
				float *n = (*pt) + 2*ni;
				float dx = fabsf(childx - n[0]);
				float dy = fabsf(childy - n[1]);
				/*
				 * Not perfect; attempt to handle edge
				 * wrapping by determining if these cells are
				 * so far away they must be wrapped. If we're
				 * searching for neighbors 2 cells away then a
				 * neighbor can be at most 3 full cells away.
				 */
				if (dx > cl * 3)
					dx = w - dx;
				if (dy > cl * 3)
					dy = h - dy;
				if (hypotf(dx, dy) < r)
					intersect = 1;
			}

			/* This point is within radius of a neighbor */
			if (intersect)
				continue;

			/* This is a valid point */
			(*pt)[npt*2+0] = childx;
			(*pt)[npt*2+1] = childy;
			active[nactive ++] = npt;
			lookup[gridi] = npt;
			++ npt;
		}
	} while (nactive && npt < mxnpt);

	/* Reclaimed unused points */
	float *newpt = xrealloc(*pt, 2 * npt * sizeof **pt);
	*pt = newpt;
	*ptsz = npt;

	free(active);
	free(lookup);
}
