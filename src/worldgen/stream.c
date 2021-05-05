#include "hammer/worldgen/stream.h"
#include "hammer/error.h"
#include "hammer/math.h"
#include "hammer/mem.h"
#include "hammer/poisson.h"
#include "hammer/ring.h"
#include "hammer/vector.h"
#include "hammer/worldgen/climate.h"
#include "hammer/worldgen/tectonic.h"
#include <delaunay/delaunay.h>
#include <delaunay/helper.h>
#include <float.h>
#include <string.h>

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

#define STREAM_TIMESTEP 0.01f
#define POISSON_RADIUS 4.0f
#define POISSON_AREA (POISSON_RADIUS * POISSON_RADIUS * M_PI)

/* Most significant bit used for lake identification in trees vector */
//#define TREE_LAKE_BIT ((uint32_t)1<<31)
//#define TREE_NODE_MASK (((uint32_t)-1)^TREE_LAKE_BIT)
#define NO_NODE ((uint32_t)-1)

static void node_lerp_uplift(const struct climate *, unsigned long size, struct stream_node *);
static uint32_t assign_tree(struct stream_graph *g, uint32_t nid);
static uint32_t next_tree(struct stream_graph *g);
static void flow_drainage_area(struct stream_graph *g, uint32_t ni);
static void stream_power(struct stream_graph *g, uint32_t ni);

void
stream_graph_create(struct stream_graph *g,
                    const struct climate *climate,
                    unsigned long long seed,
                    unsigned long size)
{
	uint32_t *delaunay = NULL;
	float *pt = NULL;
	size_t  npt = 0;

	/* Calculate poisson distribution over map */
	poisson(&pt, &npt, seed, POISSON_RADIUS, size, size);
	if (npt >= NO_NODE || npt >= UINT32_MAX / 9)
		xpanic("Number of points exceeds uint32_t");

	/*
	 * This is a super crude way of tiling a Delaunay triangulation:
	 * transform the domain 9 times around center. I'm pretty confident
	 * this doesn't even work, but it's sure better than no triangulation
	 * about the hull. I don't even know how we would go about doing this
	 * properly. It would involve a projection onto a sphere or a toroid.
	 * Some domain where we would be left with a single polygonal gap.
	 * Fuck that math.
	 */
	uint32_t nwrapped = npt * 9;
	float *wrapped  = xmalloc(nwrapped * 2 * sizeof(*wrapped));
	/* Our center points are [0,npt) */
	memcpy(wrapped, pt, npt * 2 * sizeof(float));
	/* And our 8 Moore neighbors, in no particular order */
	float fsize = size;
	float neighbor_offsets[8][2] = {
		{ -fsize, -fsize },
		{ -fsize,      0 },
		{ -fsize, +fsize },

		{      0, -fsize },
		{      0, +fsize },

		{ +fsize, -fsize },
		{ +fsize,      0 },
		{ +fsize, +fsize },
	};
	for (uint32_t n = 0; n < 8; ++ n)
	for (uint32_t i = 0; i < npt; ++ i) {
		float *off = neighbor_offsets[n];
		wrapped[((n+1) * npt + i)*2+0] = pt[i*2+0] + off[0];
		wrapped[((n+1) * npt + i)*2+1] = pt[i*2+1] + off[1];
	}

	/* Triangulate distribution */
	delaunay = xcalloc(DELAUNAY_SZ(nwrapped), sizeof(*delaunay));
	if (triangulate(delaunay, wrapped, nwrapped) != 0)
		xpanic("Error triangulating Poisson distribution");

	g->node_count = npt;
	g->nodes = xcalloc(g->node_count, sizeof(*g->nodes));
	g->arcs  = xcalloc(g->node_count, sizeof(*g->arcs));
	g->edges = NULL;
	g->tris  = NULL;
	g->trees = NULL;
	g->generation = 0;
	g->size = size;

	/* Create stream nodes (only center domain obviously) */
	for (uint32_t i = 0; i < g->node_count; ++ i) {
		g->arcs[i].receiver = NO_NODE;
		struct stream_node *n = &g->nodes[i];
		n->tree = NO_NODE;
		n->x = pt[i*2+0];
		n->y = pt[i*2+1];
		n->height = 0;
		n->generation = 0;
		node_lerp_uplift(climate, size, n);
	}

	/*
	 * Create unique stream edges. We're going from half-edges (arcs) to
	 * bi-directional edges here because the direction of flow between two
	 * nodes may change and this is just easier. To reconcile half-edges
	 * only create edges where the source point ID is less than the
	 * destination point ID.
	 */
	uint32_t *triverts =  DELAUNAY_TRIVERTS(delaunay, nwrapped);
	uint32_t ntris     = *DELAUNAY_NTRIVERT(delaunay, nwrapped) / 3;
	uint32_t e[3];
	for (uint32_t t = 0; t < ntris; ++ t) {
		tri_edges(t, e);
		uint32_t a = triverts[e[0]];
		uint32_t b = triverts[e[1]];
		uint32_t c = triverts[e[2]];
		/* Discard edges completely out of center domain */
		if (a >= npt && b >= npt && c >= npt)
			continue;
		/*
		 * Wrap back to center domain from our Moore neighborhood,
		 * mathematically inept travesty BEFORE culling half-edges.
		 */
		a %= npt;
		b %= npt;
		c %= npt;
		if (a < b)
			vector_push(&g->edges, (struct stream_edge) { a, b });
		if (b < c)
			vector_push(&g->edges, (struct stream_edge) { b, c });
		if (c < a)
			vector_push(&g->edges, (struct stream_edge) { c, a });
		vector_push(&g->tris, (struct stream_tri) { a, b, c });
	}

	free(delaunay);
	free(wrapped);
	free(pt);
}

void
stream_graph_destroy(struct stream_graph *g)
{
	free(g->nodes);
	free(g->arcs);
	vector_free(&g->edges);
	vector_free(&g->tris);
	size_t tree_count = vector_size(g->trees);
	for (size_t t = 0; t < tree_count; ++ t)
		vector_free(&g->trees[t].border_edges);
	vector_free(&g->trees);
}

void
stream_graph_update(struct stream_graph *g)
{
	++ g->generation;

	/* Delete old trees */
	{
		size_t tree_count = vector_size(g->trees);
		for (size_t t = 0; t < tree_count; ++ t)
			vector_free(&g->trees[t].border_edges);
		vector_clear(&g->trees);
	}
	for (uint32_t i = 0; i < g->node_count; ++ i) {
		g->nodes[i].tree = NO_NODE;
		g->arcs[i].receiver = NO_NODE;
	}

	/* Generate stream arcs */
	uint32_t edge_count = vector_size(g->edges);
	for (uint32_t ei = 0; ei < edge_count; ++ ei) {
		struct stream_edge *e = &g->edges[ei];
		uint32_t src;
		uint32_t dst;
		float delta = g->nodes[e->a].height - g->nodes[e->b].height;
		/* Do not use FLT_EPSILON; results in many lakes */
		if (delta == 0)
			continue;

		if (delta > 0) {
			src = e->a;
			dst = e->b;
		} else {
			src = e->b;
			dst = e->a;
		}

		/*
		 * If this node has no receiver, assign dst as receiver.
		 * Otherwise, determine which potential receiver is lower and
		 * choose that receiver
		 */
		if (g->arcs[src].receiver == NO_NODE) {
			g->arcs[src].receiver = dst;
			continue;
		}
		struct stream_node *dstn = &g->nodes[dst];
		struct stream_node *rcvn = &g->nodes[g->arcs[src].receiver];
		if (rcvn->height > dstn->height)
			g->arcs[src].receiver = dst;
	}

	/* Travel down to root to determine or create tree, identify lakes */
	uint32_t *border_trees = NULL; /* ring buffer */
	for (size_t ni = 0; ni < g->node_count; ++ ni)
		assign_tree(g, ni);

	uint32_t tree_count = vector_size(g->trees);
	for (uint32_t ti = 0; ti < tree_count; ++ ti) {
		struct stream_tree *t = &g->trees[ti];
		if (t->is_lake)
			ring_push(&border_trees, ti);
	}

	/* Assign edges to trees */
	for (uint32_t ei = 0; ei < edge_count; ++ ei) {
		struct stream_edge *e = &g->edges[ei];
		struct stream_node *a = &g->nodes[e->a];
		struct stream_node *b = &g->nodes[e->b];
		if (a->tree == b->tree)
			continue;
		struct stream_tree *at = &g->trees[a->tree];
		struct stream_tree *bt = &g->trees[b->tree];
		/* Don't flow from lakes */
		if (!bt->is_lake)
			vector_push(&at->border_edges, ei);
		if (!at->is_lake)
			vector_push(&bt->border_edges, ei);
	}

	while (ring_size(border_trees)) {
		uint32_t li = *ring_head(border_trees);
		ring_pop(&border_trees);
		struct stream_tree *lake_tree = &g->trees[li];
		lake_tree->in_queue = 0; /* Mark as eligible for optimization */
		size_t border_count = vector_size(lake_tree->border_edges);
		for (uint32_t ei = 0; ei < border_count; ++ ei) {
			struct stream_edge *e = &g->edges[lake_tree->border_edges[ei]];
			uint32_t lake_nid = e->a;
			uint32_t othr_nid = e->b;
			struct stream_node *lake_node = &g->nodes[lake_nid];
			struct stream_node *othr_node = &g->nodes[othr_nid];
			/* Swap if necessary */
			if (othr_node->tree == li) {
				struct stream_node *tmpn = lake_node;
				lake_node = othr_node;
				othr_node = tmpn;
				uint32_t tmpnid = lake_nid;
				lake_nid = othr_nid;
				othr_nid = tmpnid;
			}
			/* Determine whether other flows into this lake */
			struct stream_tree *n = &g->trees[othr_node->tree];
			float pass = lake_tree->pass_to_receiver +
			             MAX(lake_node->height, othr_node->height);
			if (pass < n->pass_to_receiver) {
				n->pass_to_receiver = pass;
				n->lake_receiver = li;
				n->node_receiver = lake_nid;
			}
			if (!n->in_queue) {
				n->in_queue = 1;
				ring_push(&border_trees, othr_node->tree);
			}
		}
		vector_free(&lake_tree->border_edges);
	}

	ring_free(&border_trees);

	/* XXX Sum total pass height for lake surface height info */

	/* Create arcs from passes */
	for (uint32_t ti = 0; ti < tree_count; ++ ti) {
		struct stream_tree *t = &g->trees[ti];
		if (t->node_receiver != NO_NODE)
			g->arcs[t->root].receiver = t->node_receiver;
	}

	/* Calculate drainage area */
	for (uint32_t ni = 0; ni < g->node_count; ++ ni)
		flow_drainage_area(g, ni);

	/* Calculate slope and perform thermal erosion */
	for (uint32_t ni = 0; ni < g->node_count; ++ ni) {
		struct stream_arc *arc = &g->arcs[ni];
		if (arc->receiver == NO_NODE)
			continue;

		struct stream_node *src = &g->nodes[ni];
		struct stream_node *dst = &g->nodes[arc->receiver];
		float d = src->height - dst->height;
		const float talus = 0.2f;
		if (d > talus) {
			float xfer = (d - talus) / 2;
			dst->height += xfer;
			src->height -= xfer;
		}
	}

	/*
	 * I really don't understand the stream power equation, so I can't
	 * rightfully say that's what I'm calculating here...
	 */
	for (uint32_t ni = 0; ni < g->node_count; ++ ni)
		stream_power(g, ni);
}

static void
node_lerp_uplift(const struct climate *c,
                 unsigned long size,
                 struct stream_node *n)
{
	float scale = size / (float)CLIMATE_LEN;
	assert(scale >= 1);
	float cx = n->x / scale;
	float cy = n->y / scale;
	long cxf = wrapidx(floorf(cx), CLIMATE_LEN);
	long cxc = wrapidx( ceilf(cx), CLIMATE_LEN);
	long cyf = wrapidx(floorf(cy), CLIMATE_LEN);
	long cyc = wrapidx( ceilf(cy), CLIMATE_LEN);
	float uff = c->uplift[cyf * CLIMATE_LEN + cxf];
	float ufc = c->uplift[cyf * CLIMATE_LEN + cxc];
	float ucf = c->uplift[cyc * CLIMATE_LEN + cxf];
	float ucc = c->uplift[cyc * CLIMATE_LEN + cxc];
	float uf = uff + (ufc - uff) * (cx - cxf);
	float uc = ucf + (ucc - ucf) * (cx - cxf);
	n->uplift = uf + (uc - uf) * (cy - cyf);
}

static uint32_t
assign_tree(struct stream_graph *g, uint32_t nid)
{
	struct stream_node *n = &g->nodes[nid];

	if (n->tree != NO_NODE)
		return n->tree;

	struct stream_arc *arc = &g->arcs[nid];
	if (arc->receiver == NO_NODE) {
		uint32_t t = next_tree(g);
		n->tree = t;
		int is_lake = n->height < TECTONIC_CONTINENT_MASS;
		g->trees[t] = (struct stream_tree) {
			.border_edges = NULL,
			.lake_receiver = NO_NODE,
			.node_receiver = NO_NODE,
			.root = nid,
			.pass_to_receiver = is_lake ? 0 : FLT_MAX,
			.in_queue = 0,
			.is_lake = is_lake
		};
		return t;
	}

	uint32_t t = assign_tree(g, arc->receiver);
	n->tree = t;
	return t;
}

static uint32_t
next_tree(struct stream_graph *g)
{
	uint32_t t = vector_size(g->trees);
	vector_pushz(&g->trees);
	return t;
}

static void
flow_drainage_area(struct stream_graph *g, uint32_t ni)
{
	do {
		struct stream_node *n = &g->nodes[ni];
		/* Reduce drainage below ocean level */
		if (n->height >= TECTONIC_CONTINENT_MASS)
			n->drainage += POISSON_AREA;
		else
			n->drainage += POISSON_AREA *
			               (n->height / TECTONIC_CONTINENT_MASS);
		ni = g->arcs[ni].receiver;
	} while (ni != NO_NODE);
}

static void
stream_power(struct stream_graph *g, uint32_t ni)
{
	struct stream_node *n = &g->nodes[ni];
	struct stream_arc *arc = &g->arcs[ni];

	if (n->generation == g->generation)
		return;

	if (arc->receiver == NO_NODE) {
		n->height += n->uplift * STREAM_TIMESTEP;
	} else {
		stream_power(g, arc->receiver);
		float receiver_height = g->nodes[arc->receiver].height;
		float ero = 0.5f * sqrtf(n->drainage) / (2 * POISSON_RADIUS);
		n->height += STREAM_TIMESTEP * (n->uplift + ero * receiver_height);
		n->height /= 1 + ero * STREAM_TIMESTEP;
	}

	n->generation = g->generation;
}
