#include "hammer/appstate/viz_stream.h"
#include "hammer/appstate/world_config.h"
#include "hammer/glthread.h"
#include "hammer/gui.h"
#include "hammer/math.h"
#include "hammer/mem.h"
#include "hammer/vector.h"
#include "hammer/window.h"
#include "hammer/worldgen/stream.h"
#include "hammer/worldgen/tectonic.h"

#include "hammer/image.h" /* XXX Debug */

#define PROGRESS_STR_MAX_LEN 64

struct viz_stream_appstate {
	dltask task;
	struct world_opts opts;
	struct stream_graph stream_graph;
	gui_btn_state cancel_btn_state;
	int show_streams;
	GLuint img;
	char progress_str[PROGRESS_STR_MAX_LEN];
};

static void viz_stream_entry(DL_TASK_ARGS);
static void viz_stream_exit (DL_TASK_ARGS);
static void viz_stream_loop (DL_TASK_ARGS);

static int viz_stream_gl_setup(void *);
static int viz_stream_gl_frame(void *);

static void putline(GLubyte *rgb, unsigned size,
                    GLubyte r, GLubyte g, GLubyte b,
                    long x0, long y0, long x1, long y1);
static void blit_to_image(struct viz_stream_appstate *);

dltask *
viz_stream_appstate_alloc_detached(const struct world_opts *opts,
                                   const struct climate    *c)
{
	struct viz_stream_appstate *viz = xmalloc(sizeof(*viz));

	viz->task = DL_TASK_INIT(viz_stream_entry);
	viz->opts = *opts;
	stream_graph_create(&viz->stream_graph, c, opts->seed, opts->size);

	return &viz->task;
}

static void
viz_stream_entry(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct viz_stream_appstate, viz, task);

	glthread_execute(viz_stream_gl_setup, viz);
	viz->cancel_btn_state = 0;
	viz->show_streams = 0;

	dltail(&viz->task, viz_stream_loop);
}

static void
viz_stream_exit(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct viz_stream_appstate, viz, task);
	stream_graph_destroy(&viz->stream_graph);
	free(viz);
}

static void
viz_stream_loop(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct viz_stream_appstate, viz, task);

	if (glthread_execute(viz_stream_gl_frame, viz) ||
	    viz->cancel_btn_state == GUI_BTN_RELEASED)
	{
		dltail(&viz->task, viz_stream_exit);
		return;
	}

	if (viz->stream_graph.generation < STREAM_GRAPH_GENERATIONS) {
		/* Rendering is SUPER expensive, relative to generating */
		for (int i = 0; i < 10; ++ i)
			stream_graph_update(&viz->stream_graph);
	} else {
		/* XXX Next step */
	}

	dltail(&viz->task, viz_stream_loop);
}

static int
viz_stream_gl_setup(void *viz_)
{
	struct viz_stream_appstate *viz = viz_;

	glClearColor(49 / 255.0f, 59 / 255.0f, 58 / 255.0f, 1);

	glGenTextures(1, &viz->img);
	glBindTexture(GL_TEXTURE_2D, viz->img);
	glTexImage2D(GL_TEXTURE_2D, 0, /* level */
	                            GL_RGB8,
	                            viz->opts.size, viz->opts.size,
	                            0, /* border */
	                            GL_RGB,
	                            GL_UNSIGNED_BYTE,
	                            NULL);
	glTextureParameteri(viz->img, GL_TEXTURE_MAX_LEVEL, 0);
	glTextureParameteri(viz->img, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(viz->img, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteri(viz->img, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(viz->img, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glGenerateTextureMipmap(viz->img);

	return 0;
}

static int
viz_stream_gl_frame(void *viz_)
{
	struct viz_stream_appstate *viz = viz_;

	window_startframe();

	blit_to_image(viz);

	unsigned font_size = 24;
	unsigned padding = 32;

	const char *title = "Stream Graph Generation";
	const char *cancel_btn_text = "Cancel";
	const char *show_streams_text = "Show streams";

	unsigned btn_height = font_size + 16;
	unsigned cancel_btn_width = window.width / 2;
	unsigned check_size = font_size + 2;

	snprintf(viz->progress_str,
	         PROGRESS_STR_MAX_LEN,
	         "Generation: %ld/%ld",
	         (long)viz->stream_graph.generation,
	         (long)STREAM_GRAPH_GENERATIONS);

	struct text_opts title_opts = {
		TEXT_OPTS_DEFAULTS,
		.xoffset = padding,
		.yoffset = padding,
		.zoffset = 1,
		.size    = font_size,
		.weight  = 255
	};

	struct text_opts progress_opts = {
		TEXT_OPTS_DEFAULTS,
		.xoffset = padding,
		.yoffset = padding + font_size,
		.zoffset = 1,
		.size    = font_size
	};

	struct check_opts show_streams_check_opts = {
		CHECK_OPTS_DEFAULTS,
		.xoffset = padding,
		.yoffset = padding + font_size * 2,
		.zoffset = 1,
		.size    = font_size,
		.width   = check_size,
		.height  = check_size
	};

	struct text_opts show_streams_text_opts = {
		TEXT_OPTS_DEFAULTS,
		.xoffset = padding + show_streams_check_opts.width + 4,
		.yoffset = padding + font_size * 2,
		.zoffset = 1,
		.size    = font_size
	};

	float imgy = title_opts.yoffset;
	float img_height = window.height - imgy - btn_height - padding - 8;
	float dim = MIN(img_height, cancel_btn_width);

	struct img_opts img_opts = {
		IMG_OPTS_DEFAULTS,
		.xoffset = window.width - padding - dim,
		.yoffset = imgy,
		.zoffset = 0,
		.width  = dim,
		.height = dim
	};

	struct btn_opts cancel_btn_opts = {
		BTN_OPTS_DEFAULTS,
		.xoffset = img_opts.xoffset,
		.yoffset = imgy + dim + padding,
		.width   = dim,
		.height  = btn_height,
		.size    = font_size
	};

	gui_text(title, strlen(title), title_opts);
	gui_text(viz->progress_str, strlen(viz->progress_str), progress_opts);

	viz->show_streams = gui_check(viz->show_streams,
	                              show_streams_check_opts);
	gui_text(show_streams_text,
	         strlen(show_streams_text),
	         show_streams_text_opts);

	viz->cancel_btn_state = gui_btn(viz->cancel_btn_state,
	                                cancel_btn_text,
	                                strlen(cancel_btn_text),
	                                cancel_btn_opts);

	gui_img(viz->img, img_opts);

	window_submitframe();

	return window.should_close;
}

static void
putline(GLubyte *rgb, unsigned size,
        GLubyte r, GLubyte g, GLubyte b,
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
		rgb[i+0] = r;
		rgb[i+1] = g;
		rgb[i+2] = b;
		if (x0 == x1 && y0 == y1)
			break;
		e2 = 2 * de;
		if (e2 >= dy) { de += dy; x0 += sx; }
		if (e2 <= dx) { de += dx; y0 += sy; }
	}
}

static void
baryw(struct stream_node *n[3], float w[3], long x, long y)
{
	float d = 1 / ((n[1]->y - n[2]->y) * (n[0]->x - n[2]->x) +
	               (n[2]->x - n[1]->x) * (n[0]->y - n[2]->y));
	w[0] = d * ((n[1]->y - n[2]->y) * (x - n[2]->x) +
	            (n[2]->x - n[1]->x) * (y - n[2]->y));
	w[1] = d * ((n[2]->y - n[0]->y) * (x - n[2]->x) +
	            (n[0]->x - n[2]->x) * (y - n[2]->y));
	w[2] = 1 - w[0] - w[1];
}

static void
node_color(struct stream_node *n, float rgb[3], float w)
{
	if (n->height > TECTONIC_CONTINENT_MASS) {
		float h = n->height - TECTONIC_CONTINENT_MASS;
		rgb[0] += w * 148;
		rgb[1] += w * (148 - 20 * h);
		rgb[2] += w * 77;
	} else {
		rgb[0] += w * 50;
		rgb[1] += w * (50 + 100 * n->height / TECTONIC_CONTINENT_MASS);
		rgb[2] += w * 168;
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
blit_to_image(struct viz_stream_appstate *viz)
{
	size_t area = viz->opts.size * viz->opts.size;
	GLubyte *img = xcalloc(area * 3, sizeof(*img));

	size_t tri_count = vector_size(viz->stream_graph.tris);
	for (size_t ti = 0; ti < tri_count; ++ ti) {
		struct stream_tri *tri = &viz->stream_graph.tris[ti];
		struct stream_node *n[3] = {
			&viz->stream_graph.nodes[tri->a],
			&viz->stream_graph.nodes[tri->b],
			&viz->stream_graph.nodes[tri->c]
		};
		long l = floorf(MIN(n[0]->x, MIN(n[1]->x, n[2]->x)));
		long r =  ceilf(MAX(n[0]->x, MAX(n[1]->x, n[2]->x)));
		long t = floorf(MIN(n[0]->y, MIN(n[1]->y, n[2]->y)));
		long b =  ceilf(MAX(n[0]->y, MAX(n[1]->y, n[2]->y)));
		if (labs(l - r) > 32 || labs(t - b) > 32)
			continue; /* XXX Don't handle wrapping */
		for (long y = t; y <= b; ++ y)
		for (long x = l; x <= r; ++ x) {
			float w[3] = { 0, 0, 0 };
			baryw(n, w, x, y);
			if (w[0] < 0 || w[1] < 0 || w[2] < 0)
				continue;
			/* fast wrap since size is power of two */
			long wx = (x + viz->opts.size) & (viz->opts.size - 1);
			long wy = (y + viz->opts.size) & (viz->opts.size - 1);
			size_t i = wy * viz->opts.size + wx;
			float c[3] = { 0, 0, 0 };
			node_color(n[0], c, w[0]);
			node_color(n[1], c, w[1]);
			node_color(n[2], c, w[2]);
			img[i*3+0] = MIN(c[0], 255);
			img[i*3+1] = MIN(c[1], 255);
			img[i*3+2] = MIN(c[2], 255);
		}
	}

	if (viz->show_streams) {
		for (size_t i = 0; i < viz->stream_graph.node_count; ++ i) {
			struct stream_arc *arc = &viz->stream_graph.arcs[i];
			if (arc->receiver >= viz->stream_graph.node_count)
				continue;
			struct stream_node *src = &viz->stream_graph.nodes[i];
			struct stream_node *dst = &viz->stream_graph.nodes[arc->receiver];
			float srcx = src->x;
			float srcy = src->y;
			float dstx = dst->x;
			float dsty = dst->y;
			float deltax = fabsf(srcx - dstx);
			float deltay = fabsf(srcy - dsty);
			/* Wrapping hack to keep lines from crossing image */
			int wrapx = deltax > (float)viz->opts.size - deltax;
			int wrapy = deltay > (float)viz->opts.size - deltay;
			if (wrapx && dstx > srcx)
				srcx += viz->opts.size;
			else if (wrapx && dstx < srcx)
				dstx += viz->opts.size;
			if (wrapy && dsty > srcy)
				srcy += viz->opts.size;
			else if (wrapy && dsty < srcy)
				dsty += viz->opts.size;
			GLubyte c[3];
			swizzle(src->tree, c);
			putline(img, viz->opts.size,
			        c[0], c[1], c[2],
			        srcx, srcy,
			        dstx, dsty);
		}
	}

	/* XXX Debug
	char filename[64];
	snprintf(filename, 64, "stream%03d.png", viz->stream_graph.generation);
	write_rgb(filename, img, viz->opts.size, viz->opts.size);
	*/

	glTextureSubImage2D(viz->img,
	                    0, /* level */
	                    0, 0, /* x,y offset */
	                    viz->opts.size, viz->opts.size, /* w,h */
	                    GL_RGB, GL_UNSIGNED_BYTE,
	                    img);
	free(img);
}
