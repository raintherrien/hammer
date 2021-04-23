#include "hammer/worldgen/large_scale_world_gen.h"
#include "hammer/error.h"
#include "hammer/mem.h"
#include "hammer/poisson.h"
#include <delaunay/delaunay.h>
#include <delaunay/helper.h>
#include <stddef.h>
#include <stdlib.h>

/* XXX Stub file, under construction */

struct lswg_stream_node {
	size_t point;
	size_t receiver_node;
	size_t lake_id;
	float drainage_area;
	float elevation;
	float slope;
	/* TODO: k, m, n erosion values */
};

struct lswg_stream_lake_pass {
	size_t src_node;
	size_t dst_node;
	float height;
};

struct large_scale_world_gen_state {
	struct lswg_node *nodes;
	size_t *delaunay;
	float  *points;
	size_t  points_size;
};

void
large_scale_world_gen_create(struct large_scale_world_gen_pkg *pkg)
{
	pkg->state = xmalloc(sizeof(*pkg->state));
	/* Calculate poisson distribution over map */
	poisson(&pkg->state->points, &pkg->state->points_size,
	        pkg->seed, 16.0f, pkg->size, pkg->size);
	/* Triangulate distribution */
	pkg->state->delaunay = xcalloc(DELAUNAY_SZ(pkg->state->points_size),
	                               sizeof(*pkg->state->delaunay));
	if (triangulate(pkg->state->delaunay,
	                pkg->state->points,
	                pkg->state->points_size) != 0)
	{
		xpanic("Error triangulating Poisson distribution");
	}
	// TODO: Generate nodes, calculate drainage area
	pkg->state->nodes = NULL;
}

void
large_scale_world_gen_destroy(struct large_scale_world_gen_pkg *pkg)
{
	free(pkg->state->nodes);
	free(pkg->state->delaunay);
	free(pkg->state->points);
	free(pkg->state);
}

void
large_scale_world_gen_iteration(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct large_scale_world_gen_pkg, pkg, task);
}
