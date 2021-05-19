#include "hammer/worldgen/region.h"
#include "hammer/math.h"
#include "hammer/mem.h"
#include "hammer/worldgen/stream.h"
#include "hammer/worldgen/tectonic.h"
#include "hammer/vector.h"
#include <float.h>

static void region_blit(struct region *,
                        const struct stream_graph *);

void
region_create(struct region *r,
              unsigned stream_coord_left,
              unsigned stream_coord_top,
              unsigned stream_region_size,
              const struct stream_graph *s)
{
	r->size = REGION_UPSCALE * (size_t)stream_region_size;
	r->height = xcalloc(r->size * r->size, sizeof(*r->height));
	r->water  = xcalloc(r->size * r->size, sizeof(*r->water));
	r->stream_region_size = stream_region_size;
	r->stream_coord_left = stream_coord_left;
	r->stream_coord_top = stream_coord_top;
	region_blit(r, s);
}

void
region_destroy(struct region *r)
{
	free(r->water);
	free(r->height);
}

void
region_erode(struct region *r)
{
	(void) r;
}

static void
region_blit(struct region *r,
            const struct stream_graph *s)
{
	/*
	 * Blit height information from stream graph onto region.
	 * NOTE: This is *exactly* the same code we use to render the
	 * composite image in appstate/overworld_generation.c
	 */
	size_t tri_count = vector_size(s->tris);
	for (size_t ti = 0; ti < tri_count; ++ ti) {
		struct stream_tri *tri = &s->tris[ti];
		struct stream_node n[3] = {
			s->nodes[tri->a],
			s->nodes[tri->b],
			s->nodes[tri->c]
		};

		/*
		 * Very basic triangle discard. We have two sets of
		 * coordinates along each axis to handle wrapping. We know the
		 * base coordinates are within the map, however the farther
		 * extent may wrap. If none of the three points fall into
		 * either the x or y bounds (picture this as a plus symbol
		 * extending from the region to the extent of the map) then
		 * the triangle cannot intercept the region. This is simpler
		 * than performing triangle-AABB intersection upon all four
		 * potentially wrapped bounding boxes.
		 */
		{
			float rl0 = r->stream_coord_left;
			float rt0 = r->stream_coord_top;
			float rr0 = r->stream_coord_left + r->stream_region_size;
			float rb0 = r->stream_coord_top + r->stream_region_size;
			float rl1 = rl0 - s->size;
			float rr1 = rr0 - s->size;
			float rt1 = rt0 - s->size;
			float rb1 = rb0 - s->size;
			for (size_t i = 0; i < 3; ++ i) {
				if ((n[i].x >= rl0 && n[i].x <= rr0) ||
				    (n[i].x >= rl1 && n[i].x <= rr1) ||
				    (n[i].y >= rt0 && n[i].y <= rb0) ||
				    (n[i].y >= rt1 && n[i].y <= rb1))
				{
					goto process_triangle;
				}
			}
		}
		continue;
		process_triangle: ;


		/* Determine lake height of triangle */
		float max_lake = 0;
		for (size_t i = 0; i < 3; ++ i)
			max_lake = MAX(max_lake, s->trees[n[i].tree].pass_to_receiver);
		/*
		 * In order to draw triangles that straddle the map border we
		 * need to project those positions either negative, or beyond
		 * the bounds of the map before calculating the bounding box
		 * of the triangle.
		 *
		 * Two points on a triangle should never be further away than
		 * wrap_delta. If they are, reproject the third point by
		 * adding or subtracting the size of the stream graph.
		 */
		const float wrap_delta = 512;
		#define NORMALIZE_NODE(N,A) do {                             \
			float d1 = n[N].A - n[(N+1)%3].A;                    \
			float d2 = n[N].A - n[(N+2)%3].A;                    \
			if (fabsf(d1) > wrap_delta && fabsf(d2) > wrap_delta)\
				n[N].A -= s->size * signf(d1);               \
		} while (0)
		NORMALIZE_NODE(0, x);
		NORMALIZE_NODE(0, y);
		NORMALIZE_NODE(1, x);
		NORMALIZE_NODE(1, y);
		NORMALIZE_NODE(2, x);
		NORMALIZE_NODE(2, y);
		#undef NORMALIZE_NODE
		long left   = floorf(MIN(n[0].x, MIN(n[1].x, n[2].x)));
		long right  =  ceilf(MAX(n[0].x, MAX(n[1].x, n[2].x)));
		long top    = floorf(MIN(n[0].y, MIN(n[1].y, n[2].y)));
		long bottom =  ceilf(MAX(n[0].y, MAX(n[1].y, n[2].y)));

		/* "raw" coordinates may exceed map bounds */
		float step = 1.0f / REGION_UPSCALE;
		for (float ry = top; ry - bottom <= FLT_EPSILON; ry += step)
		for (float rx = left; rx - right <= FLT_EPSILON; rx += step) {
			const size_t region_wrap = REGION_UPSCALE * s->size;
			long wx = wrapidx(REGION_UPSCALE * (rx - r->stream_coord_left), region_wrap);
			long wy = wrapidx(REGION_UPSCALE * (ry - r->stream_coord_top), region_wrap);
			if ((size_t)wx >= r->size || (size_t)wy >= r->size)
				continue;

			float w[3] = { 0, 0, 0 };
			stream_node_barycentric_weights(&n[0], &n[1], &n[2], w, rx, ry);
			if (w[0] < 0 || w[1] < 0 || w[2] < 0)
				continue;
			float elev = n[0].height * w[0] + n[1].height * w[1] + n[2].height * w[2];
			float water = n[0].drainage / 100000.0f * w[0] +
			              n[1].drainage / 100000.0f * w[1] +
			              n[2].drainage / 100000.0f * w[2];
			if (elev < max_lake)
				water = max_lake - elev;
			if (elev < TECTONIC_CONTINENT_MASS)
				water = TECTONIC_CONTINENT_MASS - elev;

			size_t i = wy * r->size + wx;
			r->height[i] = REGION_HEIGHT_SCALE * elev;
			r->water[i] = REGION_HEIGHT_SCALE * water;
		}
	}

	float *blurring[2] = { r->height, r->water };

	/* Gaussian blur */
	const long long gkl = REGION_UPSCALE * 2;
	const long long gksz = 2*gkl+1;
	float gk[gksz];
	GAUSSIANK(gk, gksz);
	float *blur_line = xmalloc(r->size * sizeof(*blur_line));

	for (size_t l = 0; l < 2; ++ l) {
		for (size_t y = 0; y < r->size; ++ y) {
			for (size_t x = 0; x < r->size; ++ x) {
				blur_line[x] = 0;
				for (int g = 0; g < gksz; ++ g) {
					size_t i = y * r->size + MIN(x+g-gkl, r->size-1);
					blur_line[x] += gk[g] * blurring[l][i];
				}
			}
			for (size_t x = 0; x < r->size; ++ x)
				blurring[l][y * r->size + x] = blur_line[x];
		}
		for (size_t x = 0; x < r->size; ++ x) {
			for (size_t y = 0; y < r->size; ++ y) {
				blur_line[y] = 0;
				for (int g = 0; g < gksz; ++ g) {
					size_t i = MIN(y+g-gkl, r->size-1) * r->size + x;
					blur_line[y] += gk[g] * blurring[l][i];
				}
			}
			for (size_t y = 0; y < r->size; ++ y)
				blurring[l][y * r->size + x] = blur_line[y];
		}
	}

	free(blur_line);
}
