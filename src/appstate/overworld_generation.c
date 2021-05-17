#include "hammer/appstate/overworld_generation.h"
#include "hammer/appstate/region_generation.h"
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
#include "hammer/worldgen/region.h"
#include "hammer/worldgen/stream.h"
#include "hammer/worldgen/tectonic.h"

#define PROGRESS_STR_MAX_LEN 64
#define BIOME_TOOLTIP_STR_MAX_LEN 64

/* TODO: Images never freed, mouse never potentially released */

struct overworld_generation_appstate {
	dltask               task;
	struct tectonic_opts tectonic_opts;
	struct world_opts    world_opts;
	struct lithosphere  *lithosphere;
	struct climate      *climate;
	struct stream_graph *stream;
	void                *composite;
	void                *focussed_img;
	gui_btn_state        cancel_btn_state;
	unsigned long        stream_size;
	float                min_map_zoom;
	float                map_zoom;
	float                map_tran_x;
	float                map_tran_y;
	float                selected_region_x;
	float                selected_region_y;
	unsigned             selected_region_size_mag2;
	GLuint               lithosphere_img;
	GLuint               climate_img;
	GLuint               stream_img;
	GLuint               composite_img;
	int                  mouse_captured;
	int                  paused;
	char                 lithosphere_progress_str[PROGRESS_STR_MAX_LEN];
	char                 climate_progress_str[PROGRESS_STR_MAX_LEN];
	char                 stream_progress_str[PROGRESS_STR_MAX_LEN];
	char                 biome_tooltip_str[BIOME_TOOLTIP_STR_MAX_LEN];
};

static void overworld_generation_entry(DL_TASK_ARGS);
static void overworld_generation_exit(DL_TASK_ARGS);

static int overworld_generation_gl_setup(void *);
static int overworld_generation_gl_frame(void *);

static int overworld_generation_gl_blit_lithosphere_image(void *);
static int overworld_generation_gl_blit_climate_image(void *);
static int overworld_generation_gl_blit_stream_image(void *);
static int overworld_generation_gl_blit_composite_image(void *);

static void viz_lithosphere_loop(DL_TASK_ARGS);
static void viz_climate_loop(DL_TASK_ARGS);
static void viz_stream_loop(DL_TASK_ARGS);
static void viz_composite_loop(DL_TASK_ARGS);
static void post_region_generation(DL_TASK_ARGS);

dltask *
overworld_generation_appstate_alloc_detached(struct world_opts *world_opts)
{
	struct overworld_generation_appstate *wg = xmalloc(sizeof(*wg));

	wg->task = DL_TASK_INIT(overworld_generation_entry);
	/* TODO: Set tectonic params in world_config */
	wg->tectonic_opts = (struct tectonic_opts) {
		TECTONIC_OPTS_DEFAULTS,
		.seed = world_opts->seed
	};
	wg->world_opts = *world_opts;
	wg->lithosphere = NULL;
	wg->climate = NULL;
	wg->stream = NULL;
	wg->composite = NULL;
	wg->stream_size = 1 << (9 + wg->world_opts.scale);

	return &wg->task;
}

static void
overworld_generation_entry(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct overworld_generation_appstate, wg, task);

	glthread_execute(overworld_generation_gl_setup, wg);
	wg->cancel_btn_state = 0;
	wg->mouse_captured = 0;
	wg->paused = 0;
	wg->min_map_zoom = MIN((float)window.width / LITHOSPHERE_LEN,
	                       (float)window.height / LITHOSPHERE_LEN);
	wg->map_zoom = wg->min_map_zoom;
	wg->map_tran_x = 0;
	wg->map_tran_y = 0;
	wg->selected_region_x = FLT_MAX;
	wg->selected_region_y = FLT_MAX;
	wg->selected_region_size_mag2 = 0;

	/* Kick off lithosphere loop */
	wg->lithosphere = xmalloc(sizeof(*wg->lithosphere));
	wg->focussed_img = wg->lithosphere;
	lithosphere_create(wg->lithosphere, &wg->tectonic_opts);
	dltail(&wg->task, viz_lithosphere_loop);
}

static void
overworld_generation_exit(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct overworld_generation_appstate, wg, task);

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
	DL_TASK_ENTRY(struct overworld_generation_appstate, wg, task);

	if (glthread_execute(overworld_generation_gl_frame, wg) ||
	    wg->cancel_btn_state == GUI_BTN_RELEASED)
	{
		dltail(&wg->task, overworld_generation_exit);
		return;
	}

	size_t steps = wg->tectonic_opts.generations *
	                 wg->tectonic_opts.generation_steps;
	if (wg->lithosphere->generation >= steps) {
		/* Kick off climate loop */
		wg->climate = xmalloc(sizeof(*wg->climate));
		wg->focussed_img = wg->climate;
		climate_create(wg->climate, wg->lithosphere);
		dltail(&wg->task, viz_climate_loop);
		return;
	}

	if (!wg->paused) {
		lithosphere_update(wg->lithosphere, &wg->tectonic_opts);
		glthread_execute(overworld_generation_gl_blit_lithosphere_image, wg);
	}

	dltail(&wg->task, viz_lithosphere_loop);
}

static void
viz_climate_loop(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct overworld_generation_appstate, wg, task);

	if (glthread_execute(overworld_generation_gl_frame, wg) ||
	    wg->cancel_btn_state == GUI_BTN_RELEASED)
	{
		dltail(&wg->task, overworld_generation_exit);
		return;
	}

	if (wg->climate->generation >= CLIMATE_GENERATIONS) {
		/* Kick off stream loop */
		wg->stream = xmalloc(sizeof(*wg->stream));
		wg->focussed_img = wg->stream;
		stream_graph_create(wg->stream, wg->climate, wg->world_opts.seed, wg->stream_size);
		dltail(&wg->task, viz_stream_loop);
		return;
	}

	if (!wg->paused) {
		climate_update(wg->climate);
		glthread_execute(overworld_generation_gl_blit_climate_image, wg);
	}

	dltail(&wg->task, viz_climate_loop);
}

static void
viz_stream_loop(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct overworld_generation_appstate, wg, task);

	if (glthread_execute(overworld_generation_gl_frame, wg) ||
	    wg->cancel_btn_state == GUI_BTN_RELEASED)
	{
		dltail(&wg->task, overworld_generation_exit);
		return;
	}

	if (wg->stream->generation >= STREAM_GRAPH_GENERATIONS) {
		/* Kick off composite loop */
		wg->composite = wg; /* xxx any data */
		wg->focussed_img = wg->composite;
		glthread_execute(overworld_generation_gl_blit_composite_image, wg);
		dltail(&wg->task, viz_composite_loop);
		return;
	} else {
		if (!wg->paused) {
			stream_graph_update(wg->stream);
			glthread_execute(overworld_generation_gl_blit_stream_image, wg);
		}
	}

	dltail(&wg->task, viz_stream_loop);
}

static void
viz_composite_loop(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct overworld_generation_appstate, wg, task);

	if (glthread_execute(overworld_generation_gl_frame, wg) ||
	    wg->cancel_btn_state == GUI_BTN_RELEASED)
	{
		dltail(&wg->task, overworld_generation_exit);
		return;
	}

	if (wg->selected_region_x != FLT_MAX &&
	    wg->selected_region_y != FLT_MAX)
	{
		/* Kick off local region generation */
		dltask *next = region_generation_appstate_alloc_detached(
			wg->climate,
			wg->lithosphere,
			wg->stream,
			&wg->world_opts,
			wg->selected_region_x,
			wg->selected_region_y,
			STREAM_REGION_SIZE_MIN * (1 << wg->selected_region_size_mag2));
		dlcontinuation(&wg->task, post_region_generation);
		dlwait(&wg->task, 1);
		dlnext(next, &wg->task);
		dlasync(next);
		return;
	}

	dltail(&wg->task, viz_composite_loop);
}

static void
post_region_generation(DL_TASK_ARGS)
{
	DL_TASK_ENTRY(struct overworld_generation_appstate, wg, task);

	/* TODO: Check whether we chose to cancel or conitnue with region */

	wg->selected_region_x = FLT_MAX;
	wg->selected_region_y = FLT_MAX;

	dltail(&wg->task, viz_composite_loop);
}

static int
overworld_generation_gl_setup(void *wg_)
{
	struct overworld_generation_appstate *wg = wg_;

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

	glGenTextures(1, &wg->composite_img);
	glBindTexture(GL_TEXTURE_2D, wg->composite_img);
	glTexImage2D(GL_TEXTURE_2D, 0, /* level */
	                            GL_RGB8,
	                            wg->stream_size, wg->stream_size,
	                            0, /* border */
	                            GL_RGB,
	                            GL_UNSIGNED_BYTE,
	                            NULL);
	glTextureParameteri(wg->composite_img, GL_TEXTURE_MAX_LEVEL, 0);
	glTextureParameteri(wg->composite_img, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(wg->composite_img, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteri(wg->composite_img, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTextureParameteri(wg->composite_img, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glGenerateTextureMipmap(wg->composite_img);

	return 0;
}

static int
overworld_generation_gl_frame(void *wg_)
{
	struct overworld_generation_appstate *wg = wg_;

	wg->min_map_zoom = MIN((float)window.width / LITHOSPHERE_LEN,
	                       (float)window.height / LITHOSPHERE_LEN);
	wg->map_zoom = MAX(wg->min_map_zoom, wg->map_zoom);

	if (window.scroll && (window.keymod & KMOD_LCTRL ||
	                      window.keymod & KMOD_RCTRL))
	{
		wg->selected_region_size_mag2 =
			CLAMP(wg->selected_region_size_mag2 - signf(window.scroll),
			      0, MIN(wg->world_opts.scale, STREAM_REGION_SIZE_MAX_MAG2));
	}
	else if (window.scroll)
	{
		float delta = window.scroll / 10.0f;
		if (wg->map_zoom + delta < wg->min_map_zoom)
			delta = wg->min_map_zoom - wg->map_zoom;
		/*
		 * Holy. Actual. Hell. This took me hours for some reason.
		 * We're trying to zoom into the center of the screen. We
		 * first need to determine how many pixels we're losing
		 * (cropping) from both width and height. HOWFUCKINGEVER these
		 * pixels will be in IMAGE SPACE because our default zoom
		 * level normalizes the shorter axis to, in this case, 1024.
		 * We need to divide this number to normalize back to window
		 * coordinates, otherwise we won't zoom in on the center.
		 * We'll zoom in on the window coordinate of whatever the
		 * center of our image space is (512?).
		 */
		float d = wg->map_zoom * (wg->map_zoom + delta);
		float cropx = (window.width  * delta) / d;
		float cropy = (window.height * delta) / d;
		wg->map_tran_x += cropx / 2  / 1024.0f;
		wg->map_tran_y += cropy / 2 / 1024.0f;
		wg->map_zoom += delta;
	}

	if (wg->mouse_captured) {
		if (!window.mouse_held[MOUSEBM]) {
			wg->mouse_captured = 0;
			window_unlock_mouse();
		}
		wg->map_tran_x -= window.motion_x / (float)window.width  / wg->map_zoom;
		wg->map_tran_y -= window.motion_y / (float)window.height / wg->map_zoom;
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

	const struct text_opts small_text_opts = {
		TEXT_OPTS_DEFAULTS,
		.size = 20
	};

	const struct btn_opts btn_opts = {
		BTN_OPTS_DEFAULTS,
		.width = font_size * 8,
		.height = font_size + 16,
		.size = font_size
	};

	const struct check_opts check_opts = {
		BTN_OPTS_DEFAULTS,
		.size = font_size,
		.width = font_size,
		.height = font_size,
	};

	const struct map_opts map_opts = {
		MAP_OPTS_DEFAULTS,
		.zoffset = 99, /* background */
		.width  = window.width,
		.height = window.height,
		.scale_x = wg->map_zoom * (LITHOSPHERE_LEN / (float)window.width),
		.scale_y = wg->map_zoom * (LITHOSPHERE_LEN / (float)window.height),
		.tran_x = wg->map_tran_x,
		.tran_y = wg->map_tran_y
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
	gui_container_push(&stack);

	gui_text("World Generation", bold_text_opts);
	gui_stack_break(&stack);
	gui_text("Scroll to zoom - Hold middle mouse to pan", small_text_opts);
	gui_stack_break(&stack);
	char seed_label[64];
	snprintf(seed_label, 64, "Seed: %llu", wg->world_opts.seed);
	gui_text(seed_label, normal_text_opts);
	gui_stack_break(&stack);
	gui_text(wg->lithosphere_progress_str, normal_text_opts);
	gui_stack_break(&stack);
	gui_text(wg->climate_progress_str, normal_text_opts);
	gui_stack_break(&stack);
	gui_text(wg->stream_progress_str, normal_text_opts);
	gui_stack_break(&stack);
	wg->paused = gui_check(wg->paused, check_opts);
	gui_text(" Pause", normal_text_opts);
	gui_stack_break(&stack);
	wg->cancel_btn_state = gui_btn(wg->cancel_btn_state, "Cancel", btn_opts);
	gui_stack_break(&stack);

	/*
	 * Show the user what biome they're hovering over on the climate image.
	 */
	if (wg->focussed_img == wg->climate &&
	    window.mouse_x >= 0 && window.mouse_y >= 0 &&
	    window.mouse_x < window.width && window.mouse_y < window.height)
	{
		size_t imgx = window.mouse_x / wg->map_zoom + wg->map_tran_x * CLIMATE_LEN;
		size_t imgy = window.mouse_y / wg->map_zoom + wg->map_tran_y * CLIMATE_LEN;
		size_t i = wrapidx(imgy, CLIMATE_LEN) * CLIMATE_LEN + wrapidx(imgx, CLIMATE_LEN);
		float temp = 1 - wg->climate->inv_temp[i];
		float precip = wg->climate->precipitation[i];
		float elev = wg->climate->uplift[i];
		enum biome b = biome_class(elev, temp, precip);
		snprintf(wg->biome_tooltip_str,
		         BIOME_TOOLTIP_STR_MAX_LEN,
		         "Hovering over: %s",
		         biome_name[b]);
		gui_text(wg->biome_tooltip_str, normal_text_opts);
		gui_stack_break(&stack);
	}

	gui_container_pop();

	if (wg->focussed_img == wg->composite) {
		gui_map(wg->composite_img, map_opts);
	} else if (wg->composite) {
		const struct map_opts tab_opts = {
			MAP_OPTS_DEFAULTS,
			.xoffset = window.width - border_padding - 256,
			.yoffset = border_padding,
			.width  = 64,
			.height = 64,
			.zoffset = 0.5f
		};
		gui_map(wg->composite_img, tab_opts);

		if (window.unhandled_mouse_press[MOUSEBL] &&
		    window.mouse_x >= tab_opts.xoffset &&
		    window.mouse_x <  tab_opts.xoffset + tab_opts.width &&
		    window.mouse_y >= tab_opts.yoffset &&
		    window.mouse_y <  tab_opts.yoffset + tab_opts.height)
		{
			window.unhandled_mouse_press[MOUSEBL] = 0;
			wg->focussed_img = wg->composite;
		}
	}

	if (wg->focussed_img == wg->stream) {
		gui_map(wg->stream_img, map_opts);
	} else if (wg->stream) {
		const struct map_opts tab_opts = {
			MAP_OPTS_DEFAULTS,
			.xoffset = window.width - border_padding - 192,
			.yoffset = border_padding,
			.width  = 64,
			.height = 64,
			.zoffset = 0.5f
		};
		gui_map(wg->stream_img, tab_opts);

		if (window.unhandled_mouse_press[MOUSEBL] &&
		    window.mouse_x >= tab_opts.xoffset &&
		    window.mouse_x <  tab_opts.xoffset + tab_opts.width &&
		    window.mouse_y >= tab_opts.yoffset &&
		    window.mouse_y <  tab_opts.yoffset + tab_opts.height)
		{
			window.unhandled_mouse_press[MOUSEBL] = 0;
			wg->focussed_img = wg->stream;
		}
	}

	if (wg->focussed_img == wg->climate) {
		gui_map(wg->climate_img, map_opts);
	} else if (wg->climate) {
		const struct map_opts tab_opts = {
			MAP_OPTS_DEFAULTS,
			.xoffset = window.width - border_padding - 128,
			.yoffset = border_padding,
			.width  = 64,
			.height = 64,
			.zoffset = 0.5f
		};
		gui_map(wg->climate_img, tab_opts);

		if (window.unhandled_mouse_press[MOUSEBL] &&
		    window.mouse_x >= tab_opts.xoffset &&
		    window.mouse_x <  tab_opts.xoffset + tab_opts.width &&
		    window.mouse_y >= tab_opts.yoffset &&
		    window.mouse_y <  tab_opts.yoffset + tab_opts.height)
		{
			window.unhandled_mouse_press[MOUSEBL] = 0;
			wg->focussed_img = wg->climate;
		}
	}

	if (wg->focussed_img == wg->lithosphere) {
		gui_map(wg->lithosphere_img, map_opts);
	} else if (wg->lithosphere) {
		const struct map_opts tab_opts = {
			MAP_OPTS_DEFAULTS,
			.xoffset = window.width - border_padding - 64,
			.yoffset = border_padding,
			.width  = 64,
			.height = 64,
			.zoffset = 0.5f
		};
		gui_map(wg->lithosphere_img, tab_opts);

		if (window.unhandled_mouse_press[MOUSEBL] &&
		    window.mouse_x >= tab_opts.xoffset &&
		    window.mouse_x <  tab_opts.xoffset + tab_opts.width &&
		    window.mouse_y >= tab_opts.yoffset &&
		    window.mouse_y <  tab_opts.yoffset + tab_opts.height)
		{
			window.unhandled_mouse_press[MOUSEBL] = 0;
			wg->focussed_img = wg->lithosphere;
		}
	}

	/*
	 * Show the user details about a composite region and perform region
	 * selection.
	 */
	gui_container_push(&stack);
	if (wg->focussed_img == wg->composite && wg->composite) {
		gui_text("Left click to select region", normal_text_opts);
		gui_stack_break(&stack);
		gui_text("Ctrl+scroll to resize region", normal_text_opts);
		gui_stack_break(&stack);
		/* Determine the region highlighted */
		float rl, rr, rt, rb; /* world coords */
		float wrl, wrr, wrt, wrb; /* window coords */
		unsigned stream_region_size = STREAM_REGION_SIZE_MIN *
		                              (1 << wg->selected_region_size_mag2);
		{
			float scale = LITHOSPHERE_LEN / (float)wg->stream_size;
			float zoom = wg->map_zoom * scale;
			float tx = wg->map_tran_x * wg->stream_size;
			float ty = wg->map_tran_y * wg->stream_size;
			rl = window.mouse_x / zoom + tx - stream_region_size / 2;
			rt = window.mouse_y / zoom + ty - stream_region_size / 2;
			/* If we wanted to snap
			rl = floorf(rl / stream_region_size) * stream_region_size;
			rt = floorf(rt / stream_region_size) * stream_region_size;
			*/
			rr = rl + stream_region_size;
			rb = rt + stream_region_size;
			wrl = (rl - tx) * zoom;
			wrr = (rr - tx) * zoom;
			wrt = (rt - ty) * zoom;
			wrb = (rb - ty) * zoom;
		}
		gui_container_pop();
		char region_size_label[64];
		size_t region_size = REGION_UPSCALE *
		                     STREAM_REGION_SIZE_MIN *
		                     (1 << wg->selected_region_size_mag2);
		snprintf(region_size_label, 64, "%zux%zu", region_size, region_size);
		gui_text(region_size_label, (struct text_opts) {
			TEXT_OPTS_DEFAULTS,
			.xoffset = wrl+1,
			.yoffset = wrt+1,
			.zoffset = 1,
			.size = 20
		});
		gui_container_push(&stack);
		/* Highlight the region with a white box */
		gui_line(wrl, wrt, 98, 1, 0xffffffff, wrl, wrb, 98, 1, 0xffffffff);
		gui_line(wrr, wrt, 98, 1, 0xffffffff, wrr, wrb, 98, 1, 0xffffffff);
		gui_line(wrl, wrt, 98, 1, 0xffffffff, wrr, wrt, 98, 1, 0xffffffff);
		gui_line(wrl, wrb, 98, 1, 0xffffffff, wrr, wrb, 98, 1, 0xffffffff);
		if (window.unhandled_mouse_press[MOUSEBL]) {
			wg->selected_region_x = wrapidx(rl, wg->stream_size);
			wg->selected_region_y = wrapidx(rt, wg->stream_size);
		}
	}
	gui_container_pop();

	/* Handle after UI has a chance to steal mouse event */
	if (window.unhandled_mouse_press[MOUSEBM]) {
		window.unhandled_mouse_press[MOUSEBM] = 0;
		wg->mouse_captured = 1;
		window_lock_mouse();
	}

	window_submitframe();

	return window.should_close;
}

static int
overworld_generation_gl_blit_lithosphere_image(void *wg_)
{
	struct overworld_generation_appstate *wg = wg_;
	struct lithosphere *l = wg->lithosphere;
	GLubyte *img = xmalloc(LITHOSPHERE_AREA * sizeof(*img) * 3);
	/* Blue below sealevel, green to red continent altitude */
	for (size_t i = 0; i < LITHOSPHERE_AREA; ++ i) {
		float total_mass = l->mass[i].sediment +
		                   l->mass[i].metamorphic +
		                   l->mass[i].igneous;
		if (total_mass > TECTONIC_CONTINENT_MASS) {
			float h = total_mass - TECTONIC_CONTINENT_MASS;
			img[i*3+0] = 30 + 95 * MIN(4,h) / 3;
			img[i*3+1] = 125;
			img[i*3+2] = 30;
		} else {
			img[i*3+0] = 50;
			img[i*3+1] = 50 + 100 * total_mass / TECTONIC_CONTINENT_MASS;
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
overworld_generation_gl_blit_climate_image(void *wg_)
{
	struct overworld_generation_appstate *wg = wg_;
	struct climate *c = wg->climate;
	GLubyte *img = xmalloc(CLIMATE_AREA * sizeof(*img) * 3);
	for (size_t i = 0; i < CLIMATE_AREA; ++ i) {
		float temp = 1 - c->inv_temp[i];
		float precip = c->precipitation[i];
		float elev = c->uplift[i];
		enum biome b = biome_class(elev, temp, precip);
		memcpy(&img[i*3], biome_color[b], sizeof(GLubyte)*3);
	}

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

static int
overworld_generation_gl_blit_stream_image(void *wg_)
{
	struct overworld_generation_appstate *wg = wg_;
	struct stream_graph *s = wg->stream;
	size_t area = s->size * s->size;
	GLubyte *img = xcalloc(area * 3, sizeof(*img));

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
			c[0], c[1], c[2], 1.0f,
			srcx, srcy,
			dstx, dsty);
	}

	glTextureSubImage2D(wg->stream_img,
	                    0, /* level */
	                    0, 0, /* x,y offset */
	                    s->size, s->size, /* w,h */
	                    GL_RGB, GL_UNSIGNED_BYTE,
	                    img);
	free(img);

	return 0;
}

static int
overworld_generation_gl_blit_composite_image(void *wg_)
{
	struct overworld_generation_appstate *wg = wg_;
	struct stream_graph *s = wg->stream;
	GLubyte *img = xcalloc(s->size * s->size * 3, sizeof(*img));

	/*
	 * NOTE: This is *exactly* the same code we use to blit region height
	 * in worldgen/region.c
	 */
	size_t tri_count = vector_size(s->tris);
	const float cs = wg->stream_size / CLIMATE_LEN;
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
			float temp = 1 - lerp_climate_layer(wg->climate->inv_temp, wx, wy, cs);
			float precip = lerp_climate_layer(wg->climate->precipitation, wx, wy, cs);
			float elev = n[0].height * w[0] + n[1].height * w[1] + n[2].height * w[2];
			enum biome b = biome_class(elev, temp, precip);
			if (elev < max_lake)
				b = BIOME_OCEAN;
			memcpy(&img[i*3], biome_color[b], sizeof(GLubyte)*3);
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
			img[i*3+0] *= shading;
			img[i*3+1] *= shading;
			img[i*3+2] *= shading;
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
		putline(img, s->size,
			biome_color[BIOME_OCEAN][0],
			biome_color[BIOME_OCEAN][1],
			biome_color[BIOME_OCEAN][2],
			CLAMP(src->drainage / 10000.0f, 0, 1),
			srcx, srcy,
			dstx, dsty);
	}

	glTextureSubImage2D(wg->composite_img,
	                    0, /* level */
	                    0, 0, /* x,y offset */
	                    s->size, s->size, /* w,h */
	                    GL_RGB, GL_UNSIGNED_BYTE,
	                    img);
	free(img);

	return 0;
}
