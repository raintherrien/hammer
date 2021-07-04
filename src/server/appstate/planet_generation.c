#include "hammer/server/appstate/transition_table.h"
#include "hammer/server/server.h"
#include "hammer/mem.h"
#include "hammer/net.h"
#include "hammer/vector.h"
#include "hammer/worldgen/biome.h"
#include "hammer/worldgen/climate.h"
#include "hammer/worldgen/region.h"
#include "hammer/worldgen/stream.h"
#include "hammer/worldgen/tectonic.h"
#include <string.h>

static void appstate_planet_generation_async(DL_TASK_ARGS);

/*
 * planet_generation iterates whatever stage of planet generation we're in and
 * computes an RGB image of the result to be sent to the client.
 */
static struct planet_gen_payload *payload;
static size_t                     payloadsz;
static enum planet_gen_stage      next_stage;

/*
 * Functions to resize and write to planet_generation_render.
 */
static void resize_payload(int);
static void render_img_lithosphere(struct server *);
static void render_img_climate    (struct server *);
static void render_img_stream     (struct server *);
static void render_img_composite  (struct server *);

dltask *
appstate_planet_generation_enter(void *sx)
{
	struct server *s = sx;
	s->appstate_task = DL_TASK_INIT(appstate_planet_generation_async);

	/* Update status to planet generation */
	s->status = SERVER_STATUS_PLANET_GENERATION;
	s->planet_gen = xmalloc(sizeof(*s->planet_gen));
	xpinfo("Server beginning planet generation");

	/* Will be realloced as needed */
	payload = xmalloc(sizeof(*payload));
	payload->render_size = 0;
	payload->stage = PLANET_GEN_STAGE_NONE;
	next_stage = PLANET_GEN_STAGE_LITHOSPHERE;

	return &s->appstate_task;
}

void
appstate_planet_generation_exit(dltask *t)
{
	struct server *s = DL_TASK_DOWNCAST(t, struct server, appstate_task);

	free(payload);
	payload = NULL;

	/* We never made it past planet generation, free the server */
	if (s->status == SERVER_STATUS_PLANET_GENERATION) {
		if (s->planet_gen->stream) {
			stream_graph_destroy(s->planet_gen->stream);
			free(s->planet_gen->stream);
		}
		if (s->planet_gen->climate) {
			climate_destroy(s->planet_gen->climate);
			free(s->planet_gen->climate);
		}
		if (s->planet_gen->lithosphere) {
			lithosphere_destroy(s->planet_gen->lithosphere);
			free(s->planet_gen->lithosphere);
		}
		free(s->planet_gen);
		free(s);
	}
}

static void
appstate_planet_generation_async(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct server, s, appstate_task);

	/* Perform stage transitions */
	if (next_stage != payload->stage && next_stage != PLANET_GEN_STAGE_NONE) {
		payload->stage = next_stage;

		switch (next_stage) {
		case PLANET_GEN_STAGE_LITHOSPHERE:
			/* Create lithosphere */
			s->planet_gen->lithosphere = xcalloc(1, sizeof(*s->planet_gen->lithosphere));
			resize_payload(LITHOSPHERE_LEN);
			lithosphere_create(s->planet_gen->lithosphere, &s->world.opts);
			break;

		case PLANET_GEN_STAGE_CLIMATE:
			/* Create climate */
			s->planet_gen->climate = xcalloc(1, sizeof(*s->planet_gen->climate));
			climate_create(s->planet_gen->climate, s->planet_gen->lithosphere);
			resize_payload(CLIMATE_LEN);
			break;

		case PLANET_GEN_STAGE_STREAM:
			/* Create stream */
			s->planet_gen->stream = xcalloc(1, sizeof(*s->planet_gen->stream));
			size_t stream_graph_size = world_opts_stream_graph_size(&s->world.opts);
			stream_graph_create(s->planet_gen->stream,
			                    s->planet_gen->climate,
			                    s->world.opts.seed,
			                    stream_graph_size);
			resize_payload(s->planet_gen->stream->size);
			break;

		case PLANET_GEN_STAGE_COMPOSITE:
			/* Create climate */
			s->planet_gen->climate = xcalloc(1, sizeof(*s->planet_gen->climate));
			climate_create(s->planet_gen->climate, s->planet_gen->lithosphere);
			resize_payload(CLIMATE_LEN);
			break;

		case PLANET_GEN_STAGE_NONE:
			xpanic("unreachable");
			break;
		}
	}

	/* Perform generation */
	switch (payload->stage) {
	case PLANET_GEN_STAGE_LITHOSPHERE:
		/* Update and blit lithosphere */
		lithosphere_update(s->planet_gen->lithosphere, &s->world.opts);
		render_img_lithosphere(s);
		size_t steps = s->world.opts.tectonic.generations *
		               s->world.opts.tectonic.generation_steps;
		if (s->planet_gen->lithosphere->generation == steps)
			next_stage = PLANET_GEN_STAGE_CLIMATE;
		break;

	case PLANET_GEN_STAGE_CLIMATE:
		/* Update and blit climate */
		climate_update(s->planet_gen->climate);
		render_img_climate(s);
		if (s->planet_gen->climate->generation == CLIMATE_GENERATIONS)
			next_stage = PLANET_GEN_STAGE_STREAM;
		break;

	case PLANET_GEN_STAGE_STREAM:
		/* Update and blit stream */
		stream_graph_update(s->planet_gen->stream);
		render_img_stream(s);
		if (s->planet_gen->stream->generation == STREAM_GRAPH_GENERATIONS)
			next_stage = PLANET_GEN_STAGE_COMPOSITE;
		break;

	case PLANET_GEN_STAGE_COMPOSITE:
		/* Blit composite and finish */
		render_img_composite(s);
		next_stage = PLANET_GEN_STAGE_NONE;
		break;

	case PLANET_GEN_STAGE_NONE:
		xpanic("unreachable");
		break;
	}

	/* Respond to messages */
	enum netmsg_type type;
	size_t sz;
	while (server_peek(&type, &sz)) {
		switch (type) {
		/* Always respond to heartbeats */
		case NETMSG_TYPE_HEARTBEAT_SYN:
			xpinfo("Responding to client heartbeat");
			server_discard(sz);
			server_write(NETMSG_TYPE_HEARTBEAT_ACK, NULL, 0);
			break;

		/* Tell the user we're generating the planet */
		case NETMSG_TYPE_CLIENT_QUERY_SERVER_STATUS:
			xpinfo("Responding to client status query");
			server_discard(sz);
			server_write(NETMSG_TYPE_SERVER_RESPONSE_SERVER_STATUS, &s->status, sizeof(s->status));
			break;

		/* The user has requested an update! Shove our img at them */
		case NETMSG_TYPE_CLIENT_QUERY_PLANET_GENERATION_STAGE:
			xpinfova("Responding to client planet generation query with %zu byte payload", payloadsz);
			server_discard(sz);
			server_write(NETMSG_TYPE_SERVER_RESPONSE_PLANET_GENERATION_PAYLOAD, payload, payloadsz);
			break;

		default:
			errno = EINVAL;
			xperrorva("unexpected netmsg type: %d", type);
			server_discard(sz);
			break;
		}
	}
}

static void
resize_payload(int wh)
{
	payloadsz = sizeof(struct planet_gen_payload) + sizeof(unsigned char) * 3 * wh * wh;
	payload = xrealloc(payload, payloadsz);
	payload->render_size = wh;
}

static void
render_img_lithosphere(struct server *s)
{
	unsigned char *render = payload->render;
	const struct lithosphere *l = s->planet_gen->lithosphere;
	/* Blue below sealevel, green to red continent altitude */
	for (size_t i = 0; i < LITHOSPHERE_AREA; ++ i) {
		float total_mass = l->mass[i].sediment +
		                   l->mass[i].metamorphic +
		                   l->mass[i].igneous;
		if (total_mass > TECTONIC_CONTINENT_MASS) {
			float h = total_mass - TECTONIC_CONTINENT_MASS;
			render[i*3+0] = 30 + 95 * MIN(4,h) / 3;
			render[i*3+1] = 125;
			render[i*3+2] = 30;
		} else {
			render[i*3+0] = 50;
			render[i*3+1] = 50 + 100 * total_mass / TECTONIC_CONTINENT_MASS;
			render[i*3+2] = 168;
		}
	}
}

static void
render_img_climate(struct server *s)
{
	unsigned char *render = payload->render;
	struct climate *c = s->planet_gen->climate;
	for (size_t i = 0; i < CLIMATE_AREA; ++ i) {
		float temp = 1 - c->inv_temp[i];
		float precip = c->precipitation[i];
		float elev = c->uplift[i];
		enum biome b = biome_class(elev, temp, precip);
		memcpy(&render[i*3], biome_color[b], sizeof(unsigned char)*3);
	}
}

static void
putline(unsigned char *rgb, unsigned size,
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
swizzle(uint32_t i, unsigned char *rgb)
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
	rgb[0] = (unsigned char)lroundf(255 * r / 4);
	rgb[1] = (unsigned char)lroundf(255 * g / 4);
	rgb[2] = (unsigned char)lroundf(255 * b / 4);
}

static void
render_img_stream(struct server *s)
{
	unsigned char *render = payload->render;
	struct stream_graph *stream = s->planet_gen->stream;
	memset(render, 0, stream->size * stream->size * 3 * sizeof(*render));

	for (size_t i = 0; i < stream->node_count; ++ i) {
		struct stream_arc *arc = &stream->arcs[i];
		if (arc->receiver >= stream->node_count)
			continue;
		struct stream_node *src = &stream->nodes[i];
		struct stream_node *dst = &stream->nodes[arc->receiver];
		float srcx = src->x;
		float srcy = src->y;
		float dstx = dst->x;
		float dsty = dst->y;
		float deltax = fabsf(srcx - dstx);
		float deltay = fabsf(srcy - dsty);
		/* Wrapping hack to keep lines from crossing image */
		int wrapx = deltax > (float)stream->size - deltax;
		int wrapy = deltay > (float)stream->size - deltay;
		if (wrapx && dstx > srcx)
			srcx += stream->size;
		else if (wrapx && dstx < srcx)
			dstx += stream->size;
		if (wrapy && dsty > srcy)
			srcy += stream->size;
		else if (wrapy && dsty < srcy)
			dsty += stream->size;
		unsigned char c[3];
		swizzle(src->tree, c);
		putline(render, stream->size,
		        c[0], c[1], c[2], 1.0f,
		        srcx, srcy,
		        dstx, dsty);
	}
}

static void
render_img_composite(struct server *s)
{
	unsigned char *render = payload->render;
	const struct stream_graph *stream = s->planet_gen->stream;

	/*
	 * NOTE: This is *exactly* the same code we use to blit region height
	 * in worldgen/region.c
	 */
	size_t tri_count = vector_size(stream->tris);
	const float cs = world_opts_stream_graph_size(&s->world.opts) / CLIMATE_LEN;
	for (size_t ti = 0; ti < tri_count; ++ ti) {
		struct stream_tri *tri = &stream->tris[ti];
		struct stream_node n[3] = {
			stream->nodes[tri->a],
			stream->nodes[tri->b],
			stream->nodes[tri->c]
		};
		/* Determine lake height of triangle */
		float max_lake = 0;
		for (size_t i = 0; i < 3; ++ i)
			max_lake = MAX(max_lake, stream->trees[n[i].tree].pass_to_receiver);
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
				n[N].A -= stream->size * signf(d1);               \
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
			long wy = STREAM_WRAP(stream, ry);
			long wx = STREAM_WRAP(stream, rx);
			float w[3] = { 0, 0, 0 };
			stream_node_barycentric_weights(&n[0], &n[1], &n[2], w, rx, ry);
			if (w[0] < 0 || w[1] < 0 || w[2] < 0)
				continue;
			size_t i = wy * stream->size + wx;
			float temp = 1 - lerp_climate_layer(s->planet_gen->climate->inv_temp, wx, wy, cs);
			float precip = lerp_climate_layer(s->planet_gen->climate->precipitation, wx, wy, cs);
			float elev = n[0].height * w[0] + n[1].height * w[1] + n[2].height * w[2];
			enum biome b = biome_class(elev, temp, precip);
			if (elev < max_lake)
				b = BIOME_OCEAN;
			memcpy(&render[i*3], biome_color[b], sizeof(unsigned char)*3);
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
			render[i*3+0] *= shading;
			render[i*3+1] *= shading;
			render[i*3+2] *= shading;
		}
	}

	for (size_t i = 0; i < stream->node_count; ++ i) {
		struct stream_arc *arc = &stream->arcs[i];
		struct stream_node *src = &stream->nodes[i];

		/* Do not render in ocean or lake */
		if (arc->receiver >= stream->node_count ||
		    src->height < TECTONIC_CONTINENT_MASS ||
		    src->height < stream->trees[src->tree].pass_to_receiver)
			continue;

		struct stream_node *dst = &stream->nodes[arc->receiver];
		float srcx = src->x;
		float srcy = src->y;
		float dstx = dst->x;
		float dsty = dst->y;
		float deltax = fabsf(srcx - dstx);
		float deltay = fabsf(srcy - dsty);
		/* Wrapping hack to keep lines from crossing image */
		int wrapx = deltax > (float)stream->size - deltax;
		int wrapy = deltay > (float)stream->size - deltay;
		if (wrapx && dstx > srcx)
			srcx += stream->size;
		else if (wrapx && dstx < srcx)
			dstx += stream->size;
		if (wrapy && dsty > srcy)
			srcy += stream->size;
		else if (wrapy && dsty < srcy)
			dsty += stream->size;
		putline(render, stream->size,
		        biome_color[BIOME_OCEAN][0],
		        biome_color[BIOME_OCEAN][1],
		        biome_color[BIOME_OCEAN][2],
		        CLAMP(src->drainage / 10000.0f, 0, 1),
		        srcx, srcy,
		        dstx, dsty);
	}
}
