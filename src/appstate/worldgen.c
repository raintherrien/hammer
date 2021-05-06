#include "hammer/appstate/worldgen.h"
#include "hammer/appstate/world_config.h"
#include "hammer/cli.h"
#include "hammer/error.h"
#include "hammer/glthread.h"
#include "hammer/gui.h"
#include "hammer/math.h"
#include "hammer/mem.h"
#include "hammer/vector.h"
#include "hammer/window.h"
#include "hammer/worldgen/biome.h"
#include "hammer/worldgen/climate.h"
#include "hammer/worldgen/stream.h"
#include "hammer/worldgen/tectonic.h"

#include "hammer/image.h" // xxx

#define PROGRESS_STR_MAX_LEN 64
#define BIOME_TOOLTIP_STR_MAX_LEN 64

/* TODO: Images never freed, mouse never potentially released */

struct worldgen_appstate {
	dltask               task;
	struct tectonic_opts tectonic_opts;
	struct world_opts    world_opts;
	struct lithosphere  *lithosphere;
	struct climate      *climate;
	struct stream_graph *stream;
	void                *focussed_img;
	gui_btn_state        cancel_btn_state;
	unsigned long        stream_size;
	float                min_map_zoom;
	float                map_zoom;
	float                map_tran_x;
	float                map_tran_y;
	GLuint               lithosphere_img;
	GLuint               climate_img;
	GLuint               stream_img;
	int                  mouse_captured;
	int                  paused;
	char                 lithosphere_progress_str[PROGRESS_STR_MAX_LEN];
	char                 climate_progress_str[PROGRESS_STR_MAX_LEN];
	char                 stream_progress_str[PROGRESS_STR_MAX_LEN];
	char                 biome_tooltip_str[BIOME_TOOLTIP_STR_MAX_LEN];
};

static void worldgen_entry(DL_TASK_ARGS);
static void worldgen_exit(DL_TASK_ARGS);

static int worldgen_gl_setup(void *);
static int worldgen_gl_frame(void *);

static int worldgen_gl_blit_lithosphere_image(void *);
static int worldgen_gl_blit_climate_image(void *);
static int worldgen_gl_blit_stream_image(void *);

static void viz_lithosphere_loop(DL_TASK_ARGS);
static void viz_climate_loop(DL_TASK_ARGS);
static void viz_stream_loop(DL_TASK_ARGS);

dltask *
worldgen_appstate_alloc_detached(struct world_opts *world_opts)
{
	struct worldgen_appstate *wg = xmalloc(sizeof(*wg));

	wg->task = DL_TASK_INIT(worldgen_entry);
	/* TODO: Set tectonic params in world_config */
	wg->tectonic_opts = (struct tectonic_opts) {
		TECTONIC_OPTS_DEFAULTS,
		.seed = world_opts->seed
	};
	wg->world_opts = *world_opts;
	wg->lithosphere = NULL;
	wg->climate = NULL;
	wg->stream = NULL;
	wg->stream_size = 1 << (9 + wg->world_opts.scale);

	return &wg->task;
}

static void
worldgen_entry(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct worldgen_appstate, wg, task);

	glthread_execute(worldgen_gl_setup, wg);
	wg->cancel_btn_state = 0;
	wg->mouse_captured = 0;
	wg->paused = 0;
	wg->min_map_zoom = MIN((float)window.width / LITHOSPHERE_LEN,
	                       (float)window.height / LITHOSPHERE_LEN);
	wg->map_zoom = wg->min_map_zoom;
	wg->map_tran_x = 0;
	wg->map_tran_y = 0;

	/* Kick off lithosphere loop */
	wg->lithosphere = xmalloc(sizeof(*wg->lithosphere));
	wg->focussed_img = wg->lithosphere;
	lithosphere_create(wg->lithosphere, &wg->tectonic_opts);
	dltail(&wg->task, viz_lithosphere_loop);
}

static void
worldgen_exit(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct worldgen_appstate, wg, task);

	if (wg->stream) {
		stream_graph_destroy(wg->stream);
		free(wg->stream);
	}
	if (wg->climate) {
		climate_destroy(wg->climate);
		free(wg->climate);
	}
	if (wg->lithosphere) {
		lithosphere_destroy(wg->lithosphere);
		free(wg->lithosphere);
	}
	free(wg);
}

static void
viz_lithosphere_loop(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct worldgen_appstate, wg, task);

	if (glthread_execute(worldgen_gl_frame, wg) ||
	    wg->cancel_btn_state == GUI_BTN_RELEASED)
	{
		dltail(&wg->task, worldgen_exit);
		return;
	}

	size_t steps = wg->tectonic_opts.generations *
	                 wg->tectonic_opts.generation_steps;
	if (wg->lithosphere->generation >= steps)
	{
		/* Kick off climate loop */
		wg->climate = xmalloc(sizeof(*wg->climate));
		wg->focussed_img = wg->climate;
		climate_create(wg->climate, wg->lithosphere);
		dlcontinuation(&wg->task, viz_climate_loop);
		dlasync(&wg->task);
		return;
	}

	if (!wg->paused) {
		lithosphere_update(wg->lithosphere, &wg->tectonic_opts);
		glthread_execute(worldgen_gl_blit_lithosphere_image, wg);
	}

	dltail(&wg->task, viz_lithosphere_loop);
}

static void
viz_climate_loop(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct worldgen_appstate, wg, task);

	if (glthread_execute(worldgen_gl_frame, wg) ||
	    wg->cancel_btn_state == GUI_BTN_RELEASED)
	{
		dltail(&wg->task, worldgen_exit);
		return;
	}

	if (wg->climate->generation >= CLIMATE_GENERATIONS) {
		/* Kick off stream loop */
		wg->stream = xmalloc(sizeof(*wg->stream));
		wg->focussed_img = wg->stream;
		stream_graph_create(wg->stream, wg->climate, wg->world_opts.seed, wg->stream_size);
		dlcontinuation(&wg->task, viz_stream_loop);
		dlasync(&wg->task);
		return;
	}

	if (!wg->paused) {
		climate_update(wg->climate);
		glthread_execute(worldgen_gl_blit_climate_image, wg);
	}

	dltail(&wg->task, viz_climate_loop);
}

static void
viz_stream_loop(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct worldgen_appstate, wg, task);

	if (glthread_execute(worldgen_gl_frame, wg) ||
	    wg->cancel_btn_state == GUI_BTN_RELEASED)
	{
		dltail(&wg->task, worldgen_exit);
		return;
	}

	if (wg->stream->generation >= STREAM_GRAPH_GENERATIONS) {
		/* XXX Next step */
	} else {
		if (!wg->paused) {
			/* Blitting takes MUCH longer than updating */
			for (int i = 0; i < 2; ++ i)
				stream_graph_update(wg->stream);
			glthread_execute(worldgen_gl_blit_stream_image, wg);
		}
	}

	dltail(&wg->task, viz_stream_loop);
}

static int
worldgen_gl_setup(void *wg_)
{
	struct worldgen_appstate *wg = wg_;

	glClearColor(49 / 255.0f, 59 / 255.0f, 58 / 255.0f, 1);

	glGenTextures(1, &wg->lithosphere_img);
	glBindTexture(GL_TEXTURE_2D, wg->lithosphere_img);
	glTexImage2D(GL_TEXTURE_2D, 0, /* level */
	                            GL_RGB8,
	                            LITHOSPHERE_LEN, LITHOSPHERE_LEN,
	                            0, /* border */
	                            GL_RGB,
	                            GL_UNSIGNED_BYTE,
	                            NULL);
	glTextureParameteri(wg->lithosphere_img, GL_TEXTURE_MAX_LEVEL, 0);
	glTextureParameteri(wg->lithosphere_img, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(wg->lithosphere_img, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteri(wg->lithosphere_img, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTextureParameteri(wg->lithosphere_img, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glGenerateTextureMipmap(wg->lithosphere_img);

	glGenTextures(1, &wg->climate_img);
	glBindTexture(GL_TEXTURE_2D, wg->climate_img);
	glTexImage2D(GL_TEXTURE_2D, 0, /* level */
	                            GL_RGB8,
	                            CLIMATE_LEN, CLIMATE_LEN,
	                            0, /* border */
	                            GL_RGB,
	                            GL_UNSIGNED_BYTE,
	                            NULL);
	glTextureParameteri(wg->climate_img, GL_TEXTURE_MAX_LEVEL, 0);
	glTextureParameteri(wg->climate_img, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(wg->climate_img, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteri(wg->climate_img, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTextureParameteri(wg->climate_img, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glGenerateTextureMipmap(wg->climate_img);

	glGenTextures(1, &wg->stream_img);
	glBindTexture(GL_TEXTURE_2D, wg->stream_img);
	glTexImage2D(GL_TEXTURE_2D, 0, /* level */
	                            GL_RGB8,
	                            wg->stream_size, wg->stream_size,
	                            0, /* border */
	                            GL_RGB,
	                            GL_UNSIGNED_BYTE,
	                            NULL);
	glTextureParameteri(wg->stream_img, GL_TEXTURE_MAX_LEVEL, 0);
	glTextureParameteri(wg->stream_img, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(wg->stream_img, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteri(wg->stream_img, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTextureParameteri(wg->stream_img, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glGenerateTextureMipmap(wg->stream_img);

	return 0;
}

static int
worldgen_gl_frame(void *wg_)
{
	struct worldgen_appstate *wg = wg_;

	window_startframe();

	if (window.resized) {
		wg->min_map_zoom = MIN((float)window.width / LITHOSPHERE_LEN,
				       (float)window.height / LITHOSPHERE_LEN);
		wg->map_zoom = wg->min_map_zoom;
	}
	wg->map_zoom = CLAMP(wg->map_zoom + window.scroll / 100.0f, wg->min_map_zoom, 10);

	if (wg->mouse_captured) {
		if (!window.mouse_held[MOUSEBM]) {
			wg->mouse_captured = 0;
			window_unlock_mouse();
		}
		wg->map_tran_x += window.motion_x / 1000.0f / wg->map_zoom;
		wg->map_tran_y += window.motion_y / 1000.0f / wg->map_zoom;
	}
	if (window.mouse_pressed[MOUSEBM]) {
		wg->mouse_captured = 1;
		window_lock_mouse();
	}

	unsigned font_size = 24;
	unsigned border_padding = 32;
	unsigned element_padding = 8;

	const struct text_opts bold_text_opts = {
		TEXT_OPTS_DEFAULTS,
		.size = font_size,
		.weight = 255
	};

	const struct text_opts normal_text_opts = {
		TEXT_OPTS_DEFAULTS,
		.size = font_size
	};

	struct btn_opts btn_opts = {
		BTN_OPTS_DEFAULTS,
		.width = font_size * 8,
		.height = font_size + 16,
		.size = font_size
	};

	struct check_opts check_opts = {
		BTN_OPTS_DEFAULTS,
		.size = font_size,
		.width = font_size,
		.height = font_size,
	};

	snprintf(wg->lithosphere_progress_str,
	         PROGRESS_STR_MAX_LEN,
	         "Lithosphere Generation: %ld/%ld",
	         (long)wg->lithosphere->generation,
	         (long)wg->tectonic_opts.generations *
	               wg->tectonic_opts.generation_steps);

	snprintf(wg->climate_progress_str,
	         PROGRESS_STR_MAX_LEN,
	         "Climate Model Generation: %ld/%ld",
	         (long)(wg->climate ? wg->climate->generation : 0),
	         (long)CLIMATE_GENERATIONS);

	snprintf(wg->stream_progress_str,
	         PROGRESS_STR_MAX_LEN,
	         "Stream Graph Generation: %ld/%ld",
	         (long)(wg->stream ? wg->stream->generation : 0),
	         (long)STREAM_GRAPH_GENERATIONS);

	gui_container stack;
	gui_stack_init(&stack, (struct stack_opts) {
		STACK_OPTS_DEFAULTS,
		.vpadding = element_padding,
		.xoffset = border_padding,
		.yoffset = border_padding,
		.zoffset = 1
	});

	gui_text(&stack, "World Generation", bold_text_opts);
	gui_stack_break(&stack);
	gui_text(&stack, wg->lithosphere_progress_str, normal_text_opts);
	gui_stack_break(&stack);
	gui_text(&stack, wg->climate_progress_str, normal_text_opts);
	gui_stack_break(&stack);
	gui_text(&stack, wg->stream_progress_str, normal_text_opts);
	gui_stack_break(&stack);
	wg->paused = gui_check(&stack, wg->paused, check_opts);
	gui_text(&stack, " Pause", normal_text_opts);
	gui_stack_break(&stack);
	wg->cancel_btn_state = gui_btn(&stack, wg->cancel_btn_state, "Cancel", btn_opts);
	gui_stack_break(&stack);

	if (wg->focussed_img == wg->climate &&
	    window.mouse_x >= 0 && window.mouse_y >= 0 &&
	    window.mouse_x < window.width && window.mouse_y < window.height)
	{
		size_t imgx = (window.mouse_x + wg->map_tran_x) / wg->map_zoom;
		size_t imgy = (window.mouse_y + wg->map_tran_y) / wg->map_zoom;
		size_t i = wrapidx(imgy, CLIMATE_LEN) * CLIMATE_LEN + wrapidx(imgx, CLIMATE_LEN);
		float temp = 1 - wg->climate->inv_temp[i];
		float precip = wg->climate->precipitation[i];
		float elev = wg->climate->uplift[i];
		enum biome b = biome_class(elev, temp, precip);
		snprintf(wg->biome_tooltip_str,
		         BIOME_TOOLTIP_STR_MAX_LEN,
		         "Hovering over: %s",
		         biome_name[b]);
		gui_text(&stack, wg->biome_tooltip_str, normal_text_opts);
	}

	if (wg->focussed_img == wg->stream) {
		/* wrap scale, normalize to lithosphere size */
		float wx = wg->stream->size / (1 << (wg->world_opts.scale-1)) / (float)window.width;
		float wy = wg->stream->size / (1 << (wg->world_opts.scale-1)) / (float)window.height;
		gui_map(NULL, wg->stream_img, (struct map_opts) {
			MAP_OPTS_DEFAULTS,
			.width  = window.width,
			.height = window.height,
			.scale_x = wg->map_zoom * wx,
			.scale_y = wg->map_zoom * wy,
			.tran_x = wg->map_tran_x,
			.tran_y = wg->map_tran_y
		});
	} else if (wg->stream) {
		const struct map_opts tab_opts = {
			MAP_OPTS_DEFAULTS,
			.xoffset = window.width - border_padding - 192,
			.yoffset = border_padding,
			.width  = 64,
			.height = 64,
			.zoffset = 0.5f
		};
		gui_map(NULL, wg->stream_img, tab_opts);

		if (window.mouse_pressed[MOUSEBL] &&
		    window.mouse_x >= tab_opts.xoffset &&
		    window.mouse_x <  tab_opts.xoffset + tab_opts.width &&
		    window.mouse_y >= tab_opts.yoffset &&
		    window.mouse_y <  tab_opts.yoffset + tab_opts.height)
		{
			wg->focussed_img = wg->stream;
		}
	}

	if (wg->focussed_img == wg->climate) {
		/* wrap scale */
		float wx = CLIMATE_LEN / (float)window.width;
		float wy = CLIMATE_LEN / (float)window.height;
		gui_map(NULL, wg->climate_img, (struct map_opts) {
			MAP_OPTS_DEFAULTS,
			.width  = window.width,
			.height = window.height,
			.scale_x = wg->map_zoom * wx,
			.scale_y = wg->map_zoom * wy,
			.tran_x = wg->map_tran_x,
			.tran_y = wg->map_tran_y
		});
	} else if (wg->climate) {
		const struct map_opts tab_opts = {
			MAP_OPTS_DEFAULTS,
			.xoffset = window.width - border_padding - 128,
			.yoffset = border_padding,
			.width  = 64,
			.height = 64,
			.zoffset = 0.5f
		};
		gui_map(NULL, wg->climate_img, tab_opts);

		if (window.mouse_pressed[MOUSEBL] &&
		    window.mouse_x >= tab_opts.xoffset &&
		    window.mouse_x <  tab_opts.xoffset + tab_opts.width &&
		    window.mouse_y >= tab_opts.yoffset &&
		    window.mouse_y <  tab_opts.yoffset + tab_opts.height)
		{
			wg->focussed_img = wg->climate;
		}
	}

	if (wg->focussed_img == wg->lithosphere) {
		/* wrap scale */
		float wx = LITHOSPHERE_LEN / (float)window.width;
		float wy = LITHOSPHERE_LEN / (float)window.height;
		gui_map(NULL, wg->lithosphere_img, (struct map_opts) {
			MAP_OPTS_DEFAULTS,
			.width  = window.width,
			.height = window.height,
			.scale_x = wg->map_zoom * wx,
			.scale_y = wg->map_zoom * wy,
			.tran_x = wg->map_tran_x,
			.tran_y = wg->map_tran_y
		});
	} else if (wg->lithosphere) {
		const struct map_opts tab_opts = {
			MAP_OPTS_DEFAULTS,
			.xoffset = window.width - border_padding - 64,
			.yoffset = border_padding,
			.width  = 64,
			.height = 64,
			.zoffset = 0.5f
		};
		gui_map(NULL, wg->lithosphere_img, tab_opts);

		if (window.mouse_pressed[MOUSEBL] &&
		    window.mouse_x >= tab_opts.xoffset &&
		    window.mouse_x <  tab_opts.xoffset + tab_opts.width &&
		    window.mouse_y >= tab_opts.yoffset &&
		    window.mouse_y <  tab_opts.yoffset + tab_opts.height)
		{
			wg->focussed_img = wg->lithosphere;
		}
	}

	window_submitframe();

	return window.should_close;
}

static int
worldgen_gl_blit_lithosphere_image(void *wg_)
{
	struct worldgen_appstate *wg = wg_;
	struct lithosphere *l = wg->lithosphere;
	GLubyte *img = xmalloc(LITHOSPHERE_AREA * sizeof(*img) * 3);
	/* Blue below sealevel, green to red continent altitude */
	for (size_t i = 0; i < LITHOSPHERE_AREA; ++ i) {
		if (l->mass[i] > TECTONIC_CONTINENT_MASS) {
			float h = l->mass[i] - TECTONIC_CONTINENT_MASS;
			img[i*3+0] = 30 + 95 * MIN(2,h) / 1.5f;
			img[i*3+1] = 125;
			img[i*3+2] = 30;
		} else {
			img[i*3+0] = 50;
			img[i*3+1] = 50 + 100 * l->mass[i] / TECTONIC_CONTINENT_MASS;
			img[i*3+2] = 168;
		}
	}
	glTextureSubImage2D(wg->lithosphere_img,
	                    0, /* level */
	                    0, 0, /* x,y offset */
	                    LITHOSPHERE_LEN, LITHOSPHERE_LEN, /* w,h */
	                    GL_RGB, GL_UNSIGNED_BYTE,
	                    img);
	free(img);

	return 0;
}

static int
worldgen_gl_blit_climate_image(void *wg_)
{
	struct worldgen_appstate *wg = wg_;
	struct climate *c = wg->climate;
	GLubyte *img = xmalloc(CLIMATE_AREA * sizeof(*img) * 3);
	for (size_t i = 0; i < CLIMATE_AREA; ++ i) {
		float temp = 1 - c->inv_temp[i];
		float precip = c->precipitation[i];
		float elev = c->uplift[i];
		enum biome b = biome_class(elev, temp, precip);
		memcpy(&img[i*3], biome_color[b], sizeof(GLubyte)*3);
	}
/* xxx composite?
	} else {
		for (size_t i = 0; i < CLIMATE_AREA; ++ i) {
			float temperature = 1 - viz->climate.inv_temp[i];
			float freezegrad = 1 - CLAMP(2 * (temperature - CLIMATE_ARCTIC_MAX_TEMP), 0, 1);
			if (viz->climate.uplift[i] > TECTONIC_CONTINENT_MASS) {
				float h = viz->climate.uplift[i] - TECTONIC_CONTINENT_MASS;
				img[i*3+0] = 148 - 70 * MIN(1,viz->climate.precipitation[i]);
				img[i*3+1] = 148 - 20 * h;
				img[i*3+2] = 77;
				img[i*3+0] += (255 - img[i*3+0]) * freezegrad;
				img[i*3+1] += (255 - img[i*3+1]) * freezegrad;
				img[i*3+2] += (255 - img[i*3+2]) * freezegrad;
			} else {
				float temprg = (150-50) * freezegrad;
				float elevgrad = 100 * viz->climate.uplift[i] / TECTONIC_CONTINENT_MASS;
				img[i*3+0] = 50 + temprg;
				img[i*3+1] = 50 + MAX(temprg,elevgrad);
				img[i*3+2] = 168 + (255-168) * freezegrad;
			}
		}
	}
*/
	glTextureSubImage2D(wg->climate_img,
	                    0, /* level */
	                    0, 0, /* x,y offset */
	                    CLIMATE_LEN, CLIMATE_LEN, /* w,h */
	                    GL_RGB, GL_UNSIGNED_BYTE,
	                    img);
	free(img);

	return 0;
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

static int
worldgen_gl_blit_stream_image(void *wg_)
{
	struct worldgen_appstate *wg = wg_;
	struct stream_graph *s = wg->stream;
	size_t area = s->size * s->size;
	GLubyte *img = xcalloc(area * 3, sizeof(*img));

	for (size_t ni = 0; ni < s->node_count; ++ ni) {
		struct stream_node *n = &s->nodes[ni];
		GLubyte c[3];
		if (n->height > TECTONIC_CONTINENT_MASS) {
			float h = n->height - TECTONIC_CONTINENT_MASS;
			c[0] = 148;
			c[1] = MAX(0, 148 - 25 * h);
			c[2] = 77;
		} else {
			c[0] = 50;
			c[1] = (50 + 100 * n->height / TECTONIC_CONTINENT_MASS);
			c[2] = 168;
		}
		for (long y = -4; y <= 4; ++ y)
		for (long x = -4; x <= 4; ++ x) {
			/* fast wrap since size is power of two */
			long wx = (unsigned)(n->x + x + s->size) & (s->size - 1);
			long wy = (unsigned)(n->y + y + s->size) & (s->size - 1);
			size_t i = wy * s->size + wx;
			img[i*3+0] = c[0];
			img[i*3+1] = c[1];
			img[i*3+2] = c[2];
		}
	}

#if 0 /* XXX If print stream trees */
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
		putline(img, s->size,
			c[0], c[1], c[2],
			srcx, srcy,
			dstx, dsty);
	}
#endif

	/* XXX Debug
	if (s->generation == 100) {
		char filename[64];
		snprintf(filename, 64, "stream%03d.png", s->generation);
		write_rgb(filename, img, s->size, s->size);
	}
	*/

	glTextureSubImage2D(wg->stream_img,
	                    0, /* level */
	                    0, 0, /* x,y offset */
	                    s->size, s->size, /* w,h */
	                    GL_RGB, GL_UNSIGNED_BYTE,
	                    img);
	free(img);

	return 0;
}
