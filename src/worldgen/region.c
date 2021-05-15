#include "hammer/worldgen/region.h"
#include "hammer/math.h"
#include "hammer/worldgen/stream.h"
#include "hammer/vector.h"

static void region_init(float *, unsigned, unsigned, const struct stream_graph *);

void region_nh_create(struct region_nh *r,
                      unsigned x, unsigned y,
                      const struct stream_graph *s)
{
	int region_x = x / OVERWORLD_REGION_LEN;
	int region_y = y / OVERWORLD_REGION_LEN;
	r->central_index = region_y * (s->size / OVERWORLD_REGION_LEN) + region_x;

	for (size_t n = 0; n < MOORE_NEIGHBORHOOD_SIZE; ++ n) {
		const int *off = moore_neighbor_offsets[n];
		region_init(r->height[n], x + off[0], y + off[1], s);
	}
}

void region_nh_destroy(struct region_nh *r)
{
	(void) r;
}

void region_nh_erode(struct region_nh *r)
{
	(void) r;
}

static void
region_init(float *region, unsigned x, unsigned y, const struct stream_graph *s)
{
	/*
	 * Splat height information from stream graph onto region.
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
		long l = floorf(MIN(n[0].x, MIN(n[1].x, n[2].x)));
		long r =  ceilf(MAX(n[0].x, MAX(n[1].x, n[2].x)));
		long t = floorf(MIN(n[0].y, MIN(n[1].y, n[2].y)));
		long b =  ceilf(MAX(n[0].y, MAX(n[1].y, n[2].y)));
		/* "raw" coordinates may exceed map bounds */
		for (long ry = t; ry <= b; ++ ry)
		for (long rx = l; rx <= r; ++ rx) {
			long wy = STREAM_WRAP(s, ry);
			long wx = STREAM_WRAP(s, rx);
			long region_x = lroundf((wx - (float)x) / OVERWORLD_REGION_LEN * REGION_LEN);
			long region_y = lroundf((wy - (float)y) / OVERWORLD_REGION_LEN * REGION_LEN);
			/* Clip to region */
			if (region_x < 0 || region_x >= REGION_LEN ||
			    region_y < 0 || region_y >= REGION_LEN)
			{
				continue;
			}
			float w[3] = { 0, 0, 0 };
			stream_node_barycentric_weights(&n[0], &n[1], &n[2], w, rx, ry);
			if (w[0] < 0 || w[1] < 0 || w[2] < 0)
				continue;
			size_t i = region_y * REGION_LEN + region_x;
			float elev = n[0].height * w[0] + n[1].height * w[1] + n[2].height * w[2];
			if (elev < max_lake)
				region[i] = max_lake;
			else
				region[i] = elev;
		}
	}
}
