#ifndef HAMMER_WORLDGEN_STREAM_H_
#define HAMMER_WORLDGEN_STREAM_H_

#include <stddef.h>
#include <stdint.h>

struct climate;

#define STREAM_GRAPH_GENERATIONS 100

/*
 * Tectonic uplift and fluvial/thermal erosion is based upon the stream tree
 * representation and erosion model described in:
 *   Guillaume Cordonnier, Jean Braun, Marie-Paule Cani, Bedrich Benes, Eric
 *   Galin, et al.. Large Scale Terrain Generation from Tectonic Uplift and
 *   Fluvial Erosion. Computer Graphics Forum, Wiley, 2016, Proc. EUROGRAPHICS
 *   2016, 35 (2), pp.165-175. 10.1111/cgf.12820. hal-01262376
 *
 * With significant changes, simplifications.
 */

/*
 * Rather than compute the area of the Voronoi cell, assuming area to be
 * constant: the Poisson distribution.
 */
struct stream_node {
	uint32_t tree; /* index into trees vector */
	float x;
	float y;
	float height;
	float uplift;
	float drainage;
	unsigned generation;
	// TODO: local k, m, n, talus?
};

struct stream_arc {
	uint32_t receiver;
};

struct stream_edge {
	uint32_t a;
	uint32_t b;
};

struct stream_tri {
	uint32_t a;
	uint32_t b;
	uint32_t c;
};

struct stream_tree {
	uint32_t *border_edges; /* vector */
	uint32_t lake_receiver;
	uint32_t node_receiver;
	uint32_t root;
	float pass_to_receiver;
	int in_queue;
	int is_lake;
};

struct stream_graph {
	struct stream_node *nodes;
	struct stream_arc  *arcs;  /* index is source node */
	struct stream_edge *edges; /* vector */
	struct stream_tri  *tris;  /* vector */
	struct stream_tree *trees; /* vector */
	uint32_t            node_count;
	unsigned            generation;
	unsigned            size;
};

void stream_graph_create(struct stream_graph *,
                         const struct climate *,
                         unsigned long long seed,
                         unsigned long size);
void stream_graph_destroy(struct stream_graph *);
void stream_graph_update(struct stream_graph *);

#endif /* HAMMER_WORLDGEN_STREAM_H_ */
