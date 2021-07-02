#if 0
#include "hammer/client/appstate/server_planet_gen/planet_gen_iter_async.h"
#include "hammer/mem.h"
#include "hammer/vector.h"
#include "hammer/worldgen/biome.h"
#include "hammer/worldgen/climate.h"
#include "hammer/worldgen/region.h"
#include "hammer/worldgen/stream.h"
#include "hammer/worldgen/tectonic.h"
#include <stdlib.h>
#include <string.h>

static void resize_render(struct planet_gen_iter_async *, int);
static void planet_gen_iter_async_run(DL_TASK_ARGS);
static void planet_gen_iter_img_lithosphere(struct planet_gen_iter_async *);
static void planet_gen_iter_img_climate    (struct planet_gen_iter_async *);
static void planet_gen_iter_img_stream     (struct planet_gen_iter_async *);
static void planet_gen_iter_img_composite  (struct planet_gen_iter_async *);

void
planet_gen_iter_async_create(struct planet_gen_iter_async *async)
{
	async->task = DL_TASK_INIT(planet_gen_iter_async_run);
	async->iteration_render = NULL;
	async->iteration_render_width_height = 0;
	async->is_running = 0;
	async->last_stage = PLANET_STAGE_NONE;
	async->next_stage = PLANET_STAGE_LITHOSPHERE;
	async->can_resume = 1;
}

void
planet_gen_iter_async_destroy(struct planet_gen_iter_async *async)
{
	free(async->iteration_render);
	async->iteration_render = NULL;

	if (server.planet.stream) {
		stream_graph_destroy(server.planet.stream);
		free(server.planet.stream);
		server.planet.stream = NULL;
	}
	if (server.planet.climate) {
		climate_destroy(server.planet.climate);
		free(server.planet.climate);
		server.planet.climate = NULL;
	}
	if (server.planet.lithosphere) {
		lithosphere_destroy(server.planet.lithosphere);
		free(server.planet.lithosphere);
		server.planet.lithosphere = NULL;
	}
}

void
planet_gen_iter_async_resume(struct planet_gen_iter_async *async)
{
	async->is_running = 1;
	dlasync(&async->task);
}

static void
resize_render(struct planet_gen_iter_async *async, int wh)
{
	async->iteration_render_width_height = wh;
	async->iteration_render = xrealloc(async->iteration_render,
	                                   wh * wh * 3 * sizeof(*async->iteration_render));
}

static void
planet_gen_iter_async_run(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct planet_gen_iter_async, async, task);

	size_t lithosphere_steps = server.world.opts.tectonic.generations *
	                           server.world.opts.tectonic.generation_steps;

	switch (async->next_stage) {
	case PLANET_STAGE_LITHOSPHERE:
		if (async->last_stage == PLANET_STAGE_NONE) {
			/* Create lithosphere */
			server.planet.lithosphere = xcalloc(1, sizeof(*server.planet.lithosphere));
			resize_render(async, LITHOSPHERE_LEN);
			lithosphere_create(server.planet.lithosphere, &server.world.opts);
		}
		/* Update and blit lithosphere */
		lithosphere_update(server.planet.lithosphere, &server.world.opts);
		planet_gen_iter_img_lithosphere(async);
		async->last_stage = PLANET_STAGE_LITHOSPHERE;
		if (server.planet.lithosphere->generation == lithosphere_steps) {
			/* Transition to climate */
			async->next_stage = PLANET_STAGE_CLIMATE;
		}
		break;

	case PLANET_STAGE_CLIMATE:
		if (async->last_stage == PLANET_STAGE_LITHOSPHERE) {
			/* Create climate */
			server.planet.climate = xcalloc(1, sizeof(*server.planet.climate));
			climate_create(server.planet.climate, server.planet.lithosphere);
			resize_render(async, CLIMATE_LEN);
		}
		/* Update and blit climate */
		climate_update(server.planet.climate);
		planet_gen_iter_img_climate(async);
		async->last_stage = PLANET_STAGE_CLIMATE;
		if (server.planet.climate->generation == CLIMATE_GENERATIONS) {
			/* Transition to stream */
			async->next_stage = PLANET_STAGE_STREAM;
		}
		break;

	case PLANET_STAGE_STREAM:
		if (async->last_stage == PLANET_STAGE_CLIMATE) {
			/* Create stream */
			server.planet.stream = xcalloc(1, sizeof(*server.planet.stream));
			size_t stream_graph_size = world_opts_stream_graph_size(&server.world.opts);
			stream_graph_create(server.planet.stream,
					    server.planet.climate,
					    server.world.opts.seed,
					    stream_graph_size);
			resize_render(async, server.planet.stream->size);
		}
		/* Update and blit stream */
		stream_graph_update(server.planet.stream);
		planet_gen_iter_img_stream(async);
		async->last_stage = PLANET_STAGE_STREAM;
		if (server.planet.stream->generation == STREAM_GRAPH_GENERATIONS) {
			/* Transition to composite */
			async->next_stage = PLANET_STAGE_COMPOSITE;
		}
		break;

	case PLANET_STAGE_COMPOSITE:
		/* Blit composite and finish */
		planet_gen_iter_img_composite(async);
		async->last_stage = PLANET_STAGE_COMPOSITE;
		async->can_resume = 0;
		break;

	case PLANET_STAGE_NONE:
		errno = EINVAL;
		xperror("server_update_async_run invoked with PLANET_STAGE_NONE next");
		break;
	};

	/* Signal completion */
	async->is_running = 0;
}

static void
planet_gen_iter_img_lithosphere(struct planet_gen_iter_async *async)
{
	struct lithosphere *l = server.planet.lithosphere;
	/* Blue below sealevel, green to red continent altitude */
	for (size_t i = 0; i < LITHOSPHERE_AREA; ++ i) {
		float total_mass = l->mass[i].sediment +
		                   l->mass[i].metamorphic +
		                   l->mass[i].igneous;
		if (total_mass > TECTONIC_CONTINENT_MASS) {
			float h = total_mass - TECTONIC_CONTINENT_MASS;
			async->iteration_render[i*3+0] = 30 + 95 * MIN(4,h) / 3;
			async->iteration_render[i*3+1] = 125;
			async->iteration_render[i*3+2] = 30;
		} else {
			async->iteration_render[i*3+0] = 50;
			async->iteration_render[i*3+1] = 50 + 100 * total_mass / TECTONIC_CONTINENT_MASS;
			async->iteration_render[i*3+2] = 168;
		}
	}
}

static void
planet_gen_iter_img_climate(struct planet_gen_iter_async *async)
{
	struct climate *c = server.planet.climate;
	for (size_t i = 0; i < CLIMATE_AREA; ++ i) {
		float temp = 1 - c->inv_temp[i];
		float precip = c->precipitation[i];
		float elev = c->uplift[i];
		enum biome b = biome_class(elev, temp, precip);
		memcpy(&async->iteration_render[i*3], biome_color[b], sizeof(GLubyte)*3);
	}
}

static void
putline(GLubyte *rgb, unsigned size,
        float r, float g, float b, float alpha,
        long x0, long y0, long x1, long y1)
{
	/* Bresenham's line algorithm */
	long dx = +labs(x1 - x0);
	long dy = -labs(y1 - y0);
	long sx = x0 < x1 ? 1 : -1;
	long sy = y0 < y1 ? 1 : -1;
	long de = dx + dy;
	long e2 = 0; /* Accumulate error */

	for(;;) {
		/* fast wrap since size is power of two */
		long wx = (x0 + size) & (size - 1);
		long wy = (y0 + size) & (size - 1);
		size_t i = 3 * (wy * size + wx);
		rgb[i+0] = rgb[i+0] + (r - rgb[i+0]) * alpha;
		rgb[i+1] = rgb[i+1] + (g - rgb[i+1]) * alpha;
		rgb[i+2] = rgb[i+2] + (b - rgb[i+2]) * alpha;
		if (x0 == x1 && y0 == y1)
			break;
		e2 = 2 * de;
		if (e2 >= dy) { de += dy; x0 += sx; }
		if (e2 <= dx) { de += dx; y0 += sy; }
	}
}

static void
swizzle(uint32_t i, GLubyte *rgb)
{
	/*
	 * Very wonky RGB construction from the lower 12 bits of index
	 */
	float r = (((unsigned)i & (1<< 0))>> 0) +
	          (((unsigned)i & (1<< 3))>> 3) +
	          (((unsigned)i & (1<< 6))>> 6) +
	          (((unsigned)i & (1<< 9))>> 9);

	float g = (((unsigned)i & (1<< 1))>> 1) +
	          (((unsigned)i & (1<< 4))>> 4) +
	          (((unsigned)i & (1<< 7))>> 7) +
	          (((unsigned)i & (1<<10))>>10);

	float b = (((unsigned)i & (1<< 2))>> 2) +
	          (((unsigned)i & (1<< 5))>> 5) +
	          (((unsigned)i & (1<< 8))>> 8) +
	          (((unsigned)i & (1<<11))>>11);
	rgb[0] = (GLubyte)lroundf(255 * r / 4);
	rgb[1] = (GLubyte)lroundf(255 * g / 4);
	rgb[2] = (GLubyte)lroundf(255 * b / 4);
}

static void
planet_gen_iter_img_stream(struct planet_gen_iter_async *async)
{
	struct stream_graph *s = server.planet.stream;
	memset(async->iteration_render, 0, s->size * s->size * 3 * sizeof(*async->iteration_render));

	for (size_t i = 0; i < s->node_count; ++ i) {
		struct stream_arc *arc = &s->arcs[i];
		if (arc->receiver >= s->node_count)
			continue;
		struct stream_node *src = &s->nodes[i];
		struct stream_node *dst = &s->nodes[arc->receiver];
		float srcx = src->x;
		float srcy = src->y;
		float dstx = dst->x;
		float dsty = dst->y;
		float deltax = fabsf(srcx - dstx);
		float deltay = fabsf(srcy - dsty);
		/* Wrapping hack to keep lines from crossing image */
		int wrapx = deltax > (float)s->size - deltax;
		int wrapy = deltay > (float)s->size - deltay;
		if (wrapx && dstx > srcx)
			srcx += s->size;
		else if (wrapx && dstx < srcx)
			dstx += s->size;
		if (wrapy && dsty > srcy)
			srcy += s->size;
		else if (wrapy && dsty < srcy)
			dsty += s->size;
		GLubyte c[3];
		swizzle(src->tree, c);
		putline(async->iteration_render, s->size,
		        c[0], c[1], c[2], 1.0f,
		        srcx, srcy,
		        dstx, dsty);
	}
}

static void
planet_gen_iter_img_composite(struct planet_gen_iter_async *async)
{
	struct stream_graph *s = server.planet.stream;

	/*
	 * NOTE: This is *exactly* the same code we use to blit region height
	 * in worldgen/region.c
	 */
	size_t tri_count = vector_size(s->tris);
	const float cs = world_opts_stream_graph_size(&server.world.opts) / CLIMATE_LEN;
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
			float w[3] = { 0, 0, 0 };
			stream_node_barycentric_weights(&n[0], &n[1], &n[2], w, rx, ry);
			if (w[0] < 0 || w[1] < 0 || w[2] < 0)
				continue;
			size_t i = wy * s->size + wx;
			float temp = 1 - lerp_climate_layer(server.planet.climate->inv_temp, wx, wy, cs);
			float precip = lerp_climate_layer(server.planet.climate->precipitation, wx, wy, cs);
			float elev = n[0].height * w[0] + n[1].height * w[1] + n[2].height * w[2];
			enum biome b = biome_class(elev, temp, precip);
			if (elev < max_lake)
				b = BIOME_OCEAN;
			memcpy(&async->iteration_render[i*3], biome_color[b], sizeof(GLubyte)*3);
			float shading;
			/* Very basic diffuse factor [0.25,1] */
			{
				/* diagonal 45 degrees and down */
				const float lightdir[3] = {
					-1 * +0.57735026919f,
					-1 * +0.57735026919f,
					-1 * -0.57735026919f
				};
				float normal[3];
				float a[3] = {
					n[2].x - n[1].x,
					n[2].y - n[1].y,
					n[2].height - n[1].height
				};
				float b[3] = {
					n[1].x - n[0].x,
					n[1].y - n[0].y,
					n[1].height - n[0].height
				};
				normal[0] = a[1] * b[2] - a[2] * b[1];
				normal[1] = a[2] * b[0] - a[0] * b[2];
				normal[2] = a[0] * b[1] - a[1] * b[0];
				float imag = 1 / sqrtf(normal[0] * normal[0] +
				                       normal[1] * normal[1] +
				                       normal[2] * normal[2]);
				normal[0] *= imag;
				normal[1] *= imag;
				normal[2] *= imag;
				shading = normal[0] * lightdir[0] +
				          normal[1] * lightdir[1] +
				          normal[2] * lightdir[2];
				shading = shading / (2 / 0.75f) + 0.625f;
			}
			async->iteration_render[i*3+0] *= shading;
			async->iteration_render[i*3+1] *= shading;
			async->iteration_render[i*3+2] *= shading;
		}
	}

	for (size_t i = 0; i < s->node_count; ++ i) {
		struct stream_arc *arc = &s->arcs[i];
		struct stream_node *src = &s->nodes[i];

		/* Do not render in ocean or lake */
		if (arc->receiver >= s->node_count ||
		    src->height < TECTONIC_CONTINENT_MASS ||
		    src->height < s->trees[src->tree].pass_to_receiver)
			continue;

		struct stream_node *dst = &s->nodes[arc->receiver];
		float srcx = src->x;
		float srcy = src->y;
		float dstx = dst->x;
		float dsty = dst->y;
		float deltax = fabsf(srcx - dstx);
		float deltay = fabsf(srcy - dsty);
		/* Wrapping hack to keep lines from crossing image */
		int wrapx = deltax > (float)s->size - deltax;
		int wrapy = deltay > (float)s->size - deltay;
		if (wrapx && dstx > srcx)
			srcx += s->size;
		else if (wrapx && dstx < srcx)
			dstx += s->size;
		if (wrapy && dsty > srcy)
			srcy += s->size;
		else if (wrapy && dsty < srcy)
			dsty += s->size;
		putline(async->iteration_render, s->size,
		        biome_color[BIOME_OCEAN][0],
		        biome_color[BIOME_OCEAN][1],
		        biome_color[BIOME_OCEAN][2],
		        CLAMP(src->drainage / 10000.0f, 0, 1),
		        srcx, srcy,
		        dstx, dsty);
	}
}
#endif
