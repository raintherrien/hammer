#include "hammer/appstate.h"
#include "hammer/glthread.h"
#include "hammer/gui.h"
#include "hammer/mem.h"
#include "hammer/vector.h"
#include "hammer/window.h"
#include "hammer/world.h"
#include "hammer/worldgen/biome.h"
#include "hammer/worldgen/climate.h"
#include "hammer/worldgen/region.h"
#include "hammer/worldgen/stream.h"
#include "hammer/worldgen/tectonic.h"
#include <float.h>
#include <string.h>

#define PROGRESS_STR_MAX_LEN 64
#define BIOME_TOOLTIP_STR_MAX_LEN 64

dltask appstate_planet_generation_frame;

enum planet_stage {
	PLANET_STAGE_LITHOSPHERE,
	PLANET_STAGE_CLIMATE,
	PLANET_STAGE_STREAM,
	PLANET_STAGE_COMPOSITE,
};

static struct {
	/*
	 * This state performs three stages of terrain generation: tectonic,
	 * climate, and stream graph. The user can flip between rendered world
	 * images of each stage, zoom, and pan around. Eventually they will
	 * select a region to continue.
	 */
	enum planet_stage current_stage;
	enum planet_stage focussed_stage;
	gui_btn_state        cancel_btn_state;
	unsigned long        composite_size;

	float min_map_zoom;
	float map_zoom;
	float map_tran_x;
	float map_tran_y;

	float selected_region_x;
	float selected_region_y;
	unsigned selected_region_size_mag2;

	GLuint lithosphere_img;
	GLuint climate_img;
	GLuint stream_img;
	GLuint composite_img;

	int mouse_captured;
	int paused;

	char lithosphere_progress_str[PROGRESS_STR_MAX_LEN];
	char climate_progress_str[PROGRESS_STR_MAX_LEN];
	char stream_progress_str[PROGRESS_STR_MAX_LEN];
	char biome_tooltip_str[BIOME_TOOLTIP_STR_MAX_LEN];
} planet_generation;

static int planet_generation_gl_setup(void *);
static int planet_generation_gl_frame(void *);

static int planet_generation_gl_blit_lithosphere_image(void *);
static int planet_generation_gl_blit_climate_image(void *);
static int planet_generation_gl_blit_stream_image(void *);
static int planet_generation_gl_blit_composite_image(void *);

static void planet_generation_frame_async(DL_TASK_ARGS);

static void planet_generation_lithosphere_frame(void);
static void planet_generation_climate_frame(void);
static void planet_generation_stream_frame(void);
static void planet_generation_composite_frame(void);

void
appstate_planet_generation_setup(void)
{
	appstate_planet_generation_frame = DL_TASK_INIT(planet_generation_frame_async);

	world.lithosphere = xmalloc(sizeof(*world.lithosphere));
	world.climate = xmalloc(sizeof(*world.climate));
	world.stream = xmalloc(sizeof(*world.stream));
	lithosphere_create(world.lithosphere, &world.opts);

	planet_generation.current_stage = PLANET_STAGE_LITHOSPHERE;
	planet_generation.focussed_stage = PLANET_STAGE_LITHOSPHERE;
	planet_generation.cancel_btn_state = 0;
	planet_generation.composite_size = 1 << (9 + world.opts.scale);

	planet_generation.min_map_zoom = MIN((float)window.width / LITHOSPHERE_LEN,
	                                     (float)window.height / LITHOSPHERE_LEN);
	planet_generation.map_zoom = planet_generation.min_map_zoom;
	planet_generation.map_tran_x = 0;
	planet_generation.map_tran_y = 0;

	planet_generation.selected_region_x = FLT_MAX;
	planet_generation.selected_region_y = FLT_MAX;
	planet_generation.selected_region_size_mag2 = 0;

	planet_generation.mouse_captured = 0;
	planet_generation.paused = 0;

	memset(planet_generation.lithosphere_progress_str, 0, PROGRESS_STR_MAX_LEN * sizeof(char));
	memset(planet_generation.climate_progress_str, 0, PROGRESS_STR_MAX_LEN * sizeof(char));
	memset(planet_generation.stream_progress_str, 0, PROGRESS_STR_MAX_LEN * sizeof(char));
	memset(planet_generation.biome_tooltip_str, 0, PROGRESS_STR_MAX_LEN * sizeof(char));

	glthread_execute(planet_generation_gl_setup, NULL);
}

void
appstate_planet_generation_teardown(void)
{
	/* TODO: Images never freed, mouse never potentially released */

	if (world.stream) {
		if (planet_generation.current_stage >= PLANET_STAGE_STREAM)
			stream_graph_destroy(world.stream);
		free(world.stream);
		world.stream = NULL;
	}
	if (world.climate) {
		if (planet_generation.current_stage >= PLANET_STAGE_CLIMATE)
			climate_destroy(world.climate);
		free(world.climate);
		world.climate = NULL;
	}
	if (world.lithosphere) {
		lithosphere_destroy(world.lithosphere);
		free(world.lithosphere);
		world.lithosphere = NULL;
	}
}

void
appstate_planet_generation_reset_region_selection(void)
{
	planet_generation.selected_region_x = FLT_MAX;
	planet_generation.selected_region_y = FLT_MAX;
}

static void
planet_generation_frame_async(DL_TASK_ARGS)
{
	DL_TASK_ENTRY_VOID;

	if (glthread_execute(planet_generation_gl_frame, NULL) ||
	    planet_generation.cancel_btn_state == GUI_BTN_RELEASED)
	{
		appstate_transition(APPSTATE_TRANSITION_CONFIGURE_NEW_WORLD_CANCEL);
		return;
	}

	switch (planet_generation.current_stage) {
	case PLANET_STAGE_LITHOSPHERE:
		planet_generation_lithosphere_frame();
		break;
	case PLANET_STAGE_CLIMATE:
		planet_generation_climate_frame();
		break;
	case PLANET_STAGE_STREAM:
		planet_generation_stream_frame();
		break;
	case PLANET_STAGE_COMPOSITE:
		planet_generation_composite_frame();
		break;
	};
}

static void
planet_generation_lithosphere_frame(void)
{
	size_t steps = world.opts.tectonic.generations *
	                 world.opts.tectonic.generation_steps;
	if (world.lithosphere->generation >= steps) {
		/* Kick off climate loop */
		climate_create(world.climate, world.lithosphere);
		planet_generation.current_stage = PLANET_STAGE_CLIMATE;
		planet_generation.focussed_stage = PLANET_STAGE_CLIMATE;
		return;
	}

	if (!planet_generation.paused) {
		lithosphere_update(world.lithosphere, &world.opts);
		glthread_execute(planet_generation_gl_blit_lithosphere_image, NULL);
	}
}

static void
planet_generation_climate_frame(void)
{
	if (world.climate->generation >= CLIMATE_GENERATIONS) {
		/* Kick off stream loop */
		stream_graph_create(world.stream,
		                    world.climate,
		                    world.opts.seed,
		                    planet_generation.composite_size);
		planet_generation.current_stage = PLANET_STAGE_STREAM;
		planet_generation.focussed_stage = PLANET_STAGE_STREAM;
		return;
	}

	if (!planet_generation.paused) {
		climate_update(world.climate);
		glthread_execute(planet_generation_gl_blit_climate_image, NULL);
	}
}

static void
planet_generation_stream_frame(void)
{
	if (world.stream->generation >= STREAM_GRAPH_GENERATIONS) {
		/* Kick off composite loop */
		glthread_execute(planet_generation_gl_blit_composite_image, NULL);
		planet_generation.current_stage = PLANET_STAGE_COMPOSITE;
		planet_generation.focussed_stage = PLANET_STAGE_COMPOSITE;
		return;
	}

	if (!planet_generation.paused) {
		stream_graph_update(world.stream);
		glthread_execute(planet_generation_gl_blit_stream_image, NULL);
	}
}

static void
planet_generation_composite_frame(void)
{
	if (planet_generation.selected_region_x != FLT_MAX &&
	    planet_generation.selected_region_y != FLT_MAX)
	{
		/* Kick off local region generation */
		world.region_stream_coord_left = planet_generation.selected_region_x;
		world.region_stream_coord_top  = planet_generation.selected_region_y;
		world.region_size_mag2 = planet_generation.selected_region_size_mag2;
		appstate_transition(APPSTATE_TRANSITION_CHOOSE_REGION);
		return;
	}
}

static int
planet_generation_gl_setup(void *_)
{
	glClearColor(49 / 255.0f, 59 / 255.0f, 58 / 255.0f, 1);

	glGenTextures(1, &planet_generation.lithosphere_img);
	glBindTexture(GL_TEXTURE_2D, planet_generation.lithosphere_img);
	glTexImage2D(GL_TEXTURE_2D, 0, /* level */
	                            GL_RGB8,
	                            LITHOSPHERE_LEN, LITHOSPHERE_LEN,
	                            0, /* border */
	                            GL_RGB,
	                            GL_UNSIGNED_BYTE,
	                            NULL);
	glTextureParameteri(planet_generation.lithosphere_img, GL_TEXTURE_MAX_LEVEL, 0);
	glTextureParameteri(planet_generation.lithosphere_img, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(planet_generation.lithosphere_img, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteri(planet_generation.lithosphere_img, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTextureParameteri(planet_generation.lithosphere_img, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glGenerateTextureMipmap(planet_generation.lithosphere_img);

	glGenTextures(1, &planet_generation.climate_img);
	glBindTexture(GL_TEXTURE_2D, planet_generation.climate_img);
	glTexImage2D(GL_TEXTURE_2D, 0, /* level */
	                            GL_RGB8,
	                            CLIMATE_LEN, CLIMATE_LEN,
	                            0, /* border */
	                            GL_RGB,
	                            GL_UNSIGNED_BYTE,
	                            NULL);
	glTextureParameteri(planet_generation.climate_img, GL_TEXTURE_MAX_LEVEL, 0);
	glTextureParameteri(planet_generation.climate_img, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(planet_generation.climate_img, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteri(planet_generation.climate_img, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTextureParameteri(planet_generation.climate_img, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glGenerateTextureMipmap(planet_generation.climate_img);

	glGenTextures(1, &planet_generation.stream_img);
	glBindTexture(GL_TEXTURE_2D, planet_generation.stream_img);
	glTexImage2D(GL_TEXTURE_2D, 0, /* level */
	                            GL_RGB8,
	                            planet_generation.composite_size, planet_generation.composite_size,
	                            0, /* border */
	                            GL_RGB,
	                            GL_UNSIGNED_BYTE,
	                            NULL);
	glTextureParameteri(planet_generation.stream_img, GL_TEXTURE_MAX_LEVEL, 0);
	glTextureParameteri(planet_generation.stream_img, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(planet_generation.stream_img, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteri(planet_generation.stream_img, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTextureParameteri(planet_generation.stream_img, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glGenerateTextureMipmap(planet_generation.stream_img);

	glGenTextures(1, &planet_generation.composite_img);
	glBindTexture(GL_TEXTURE_2D, planet_generation.composite_img);
	glTexImage2D(GL_TEXTURE_2D, 0, /* level */
	                            GL_RGB8,
	                            planet_generation.composite_size, planet_generation.composite_size,
	                            0, /* border */
	                            GL_RGB,
	                            GL_UNSIGNED_BYTE,
	                            NULL);
	glTextureParameteri(planet_generation.composite_img, GL_TEXTURE_MAX_LEVEL, 0);
	glTextureParameteri(planet_generation.composite_img, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(planet_generation.composite_img, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteri(planet_generation.composite_img, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTextureParameteri(planet_generation.composite_img, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glGenerateTextureMipmap(planet_generation.composite_img);

	return 0;
}

static int
planet_generation_gl_frame(void *_)
{
	planet_generation.min_map_zoom = MIN((float)window.width / LITHOSPHERE_LEN,
	                       (float)window.height / LITHOSPHERE_LEN);
	planet_generation.map_zoom = MAX(planet_generation.min_map_zoom, planet_generation.map_zoom);

	if (window.scroll && (window.keymod & KMOD_LCTRL ||
	                      window.keymod & KMOD_RCTRL))
	{
		planet_generation.selected_region_size_mag2 =
			CLAMP(planet_generation.selected_region_size_mag2 - signf(window.scroll),
			      0, MIN(world.opts.scale, STREAM_REGION_SIZE_MAX_MAG2));
	}
	else if (window.scroll)
	{
		float delta = window.scroll / 10.0f;
		if (planet_generation.map_zoom + delta < planet_generation.min_map_zoom)
			delta = planet_generation.min_map_zoom - planet_generation.map_zoom;
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
		float d = planet_generation.map_zoom * (planet_generation.map_zoom + delta);
		float cropx = (window.width  * delta) / d;
		float cropy = (window.height * delta) / d;
		planet_generation.map_tran_x += cropx / 2  / 1024.0f;
		planet_generation.map_tran_y += cropy / 2 / 1024.0f;
		planet_generation.map_zoom += delta;
	}

	if (planet_generation.mouse_captured) {
		if (!window.mouse_held[MOUSEBM]) {
			planet_generation.mouse_captured = 0;
			window_unlock_mouse();
		}
		planet_generation.map_tran_x -= window.motion_x / (float)window.width  / planet_generation.map_zoom;
		planet_generation.map_tran_y -= window.motion_y / (float)window.height / planet_generation.map_zoom;
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
		.zoffset = 90, /* background */
		.width  = window.width,
		.height = window.height,
		.scale_x = planet_generation.map_zoom * (LITHOSPHERE_LEN / (float)window.width),
		.scale_y = planet_generation.map_zoom * (LITHOSPHERE_LEN / (float)window.height),
		.tran_x = planet_generation.map_tran_x,
		.tran_y = planet_generation.map_tran_y
	};

	snprintf(planet_generation.lithosphere_progress_str,
	         PROGRESS_STR_MAX_LEN,
	         "Lithosphere Generation: %ld/%ld",
	         (long)world.lithosphere->generation,
	         (long)world.opts.tectonic.generations *
	               world.opts.tectonic.generation_steps);

	snprintf(planet_generation.climate_progress_str,
	         PROGRESS_STR_MAX_LEN,
	         "Climate Model Generation: %ld/%ld",
	         (long)(world.climate ? world.climate->generation : 0),
	         (long)CLIMATE_GENERATIONS);

	snprintf(planet_generation.stream_progress_str,
	         PROGRESS_STR_MAX_LEN,
	         "Stream Graph Generation: %ld/%ld",
	         (long)(world.stream ? world.stream->generation : 0),
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
	snprintf(seed_label, 64, "Seed: %llu", world.opts.seed);
	gui_text(seed_label, normal_text_opts);
	gui_stack_break(&stack);
	gui_text(planet_generation.lithosphere_progress_str, normal_text_opts);
	gui_stack_break(&stack);
	gui_text(planet_generation.climate_progress_str, normal_text_opts);
	gui_stack_break(&stack);
	gui_text(planet_generation.stream_progress_str, normal_text_opts);
	gui_stack_break(&stack);
	planet_generation.paused = gui_check(planet_generation.paused, check_opts);
	gui_text(" Pause", normal_text_opts);
	gui_stack_break(&stack);
	planet_generation.cancel_btn_state = gui_btn(planet_generation.cancel_btn_state, "Cancel", btn_opts);
	gui_stack_break(&stack);

	/*
	 * Show the user what biome they're hovering over on the climate image.
	 */
	if (planet_generation.focussed_stage == PLANET_STAGE_CLIMATE &&
	    window.mouse_x >= 0 && window.mouse_y >= 0 &&
	    window.mouse_x < window.width && window.mouse_y < window.height)
	{
		size_t imgx = window.mouse_x / planet_generation.map_zoom + planet_generation.map_tran_x * CLIMATE_LEN;
		size_t imgy = window.mouse_y / planet_generation.map_zoom + planet_generation.map_tran_y * CLIMATE_LEN;
		size_t i = wrapidx(imgy, CLIMATE_LEN) * CLIMATE_LEN + wrapidx(imgx, CLIMATE_LEN);
		float temp = 1 - world.climate->inv_temp[i];
		float precip = world.climate->precipitation[i];
		float elev = world.climate->uplift[i];
		enum biome b = biome_class(elev, temp, precip);
		snprintf(planet_generation.biome_tooltip_str,
		         BIOME_TOOLTIP_STR_MAX_LEN,
		         "Hovering over: %s",
		         biome_name[b]);
		gui_text(planet_generation.biome_tooltip_str, normal_text_opts);
		gui_stack_break(&stack);
	}

	gui_container_pop();

	if (planet_generation.focussed_stage == PLANET_STAGE_COMPOSITE) {
		gui_map(planet_generation.composite_img, map_opts);
	} else if (planet_generation.current_stage == PLANET_STAGE_COMPOSITE) {
		const struct map_opts tab_opts = {
			MAP_OPTS_DEFAULTS,
			.xoffset = window.width - border_padding - 256,
			.yoffset = border_padding,
			.width  = 64,
			.height = 64,
			.zoffset = 0.5f
		};
		gui_map(planet_generation.composite_img, tab_opts);

		if (window.unhandled_mouse_press[MOUSEBL] &&
		    window.mouse_x >= tab_opts.xoffset &&
		    window.mouse_x <  tab_opts.xoffset + tab_opts.width &&
		    window.mouse_y >= tab_opts.yoffset &&
		    window.mouse_y <  tab_opts.yoffset + tab_opts.height)
		{
			window.unhandled_mouse_press[MOUSEBL] = 0;
			planet_generation.focussed_stage = PLANET_STAGE_COMPOSITE;
		}
	}

	if (planet_generation.focussed_stage == PLANET_STAGE_STREAM) {
		gui_map(planet_generation.stream_img, map_opts);
	} else if (planet_generation.current_stage >= PLANET_STAGE_STREAM) {
		const struct map_opts tab_opts = {
			MAP_OPTS_DEFAULTS,
			.xoffset = window.width - border_padding - 192,
			.yoffset = border_padding,
			.width  = 64,
			.height = 64,
			.zoffset = 0.5f
		};
		gui_map(planet_generation.stream_img, tab_opts);

		if (window.unhandled_mouse_press[MOUSEBL] &&
		    window.mouse_x >= tab_opts.xoffset &&
		    window.mouse_x <  tab_opts.xoffset + tab_opts.width &&
		    window.mouse_y >= tab_opts.yoffset &&
		    window.mouse_y <  tab_opts.yoffset + tab_opts.height)
		{
			window.unhandled_mouse_press[MOUSEBL] = 0;
			planet_generation.focussed_stage = PLANET_STAGE_STREAM;
		}
	}

	if (planet_generation.focussed_stage == PLANET_STAGE_CLIMATE) {
		gui_map(planet_generation.climate_img, map_opts);
	} else if (planet_generation.current_stage >= PLANET_STAGE_CLIMATE) {
		const struct map_opts tab_opts = {
			MAP_OPTS_DEFAULTS,
			.xoffset = window.width - border_padding - 128,
			.yoffset = border_padding,
			.width  = 64,
			.height = 64,
			.zoffset = 0.5f
		};
		gui_map(planet_generation.climate_img, tab_opts);

		if (window.unhandled_mouse_press[MOUSEBL] &&
		    window.mouse_x >= tab_opts.xoffset &&
		    window.mouse_x <  tab_opts.xoffset + tab_opts.width &&
		    window.mouse_y >= tab_opts.yoffset &&
		    window.mouse_y <  tab_opts.yoffset + tab_opts.height)
		{
			window.unhandled_mouse_press[MOUSEBL] = 0;
			planet_generation.focussed_stage = PLANET_STAGE_CLIMATE;
		}
	}

	if (planet_generation.focussed_stage == PLANET_STAGE_LITHOSPHERE) {
		gui_map(planet_generation.lithosphere_img, map_opts);
	} else if (planet_generation.current_stage >= PLANET_STAGE_LITHOSPHERE) {
		const struct map_opts tab_opts = {
			MAP_OPTS_DEFAULTS,
			.xoffset = window.width - border_padding - 64,
			.yoffset = border_padding,
			.width  = 64,
			.height = 64,
			.zoffset = 0.5f
		};
		gui_map(planet_generation.lithosphere_img, tab_opts);

		if (window.unhandled_mouse_press[MOUSEBL] &&
		    window.mouse_x >= tab_opts.xoffset &&
		    window.mouse_x <  tab_opts.xoffset + tab_opts.width &&
		    window.mouse_y >= tab_opts.yoffset &&
		    window.mouse_y <  tab_opts.yoffset + tab_opts.height)
		{
			window.unhandled_mouse_press[MOUSEBL] = 0;
			planet_generation.focussed_stage = PLANET_STAGE_LITHOSPHERE;
		}
	}

	/*
	 * Show the user details about a composite region and perform region
	 * selection.
	 */
	gui_container_push(&stack);
	if (planet_generation.focussed_stage == PLANET_STAGE_COMPOSITE) {
		gui_text("Left click to select region", normal_text_opts);
		gui_stack_break(&stack);
		gui_text("Ctrl+scroll to resize region", normal_text_opts);
		gui_stack_break(&stack);
		/* Determine the region highlighted */
		float stream_region_hex_size = STREAM_REGION_SIZE_MIN * (1 << planet_generation.selected_region_size_mag2);
		float stream_region_hex_width = stream_region_hex_size * 2;
		float stream_region_hex_height = stream_region_hex_size * sqrtf(3);
		float scale = LITHOSPHERE_LEN / (float)planet_generation.composite_size;
		float zoom = planet_generation.map_zoom * scale;
		float tx = planet_generation.map_tran_x * planet_generation.composite_size;
		float ty = planet_generation.map_tran_y * planet_generation.composite_size;
		/* world coords */
		float rl = window.mouse_x / zoom + tx - stream_region_hex_width / 2;
		float rt = window.mouse_y / zoom + ty - stream_region_hex_height / 2;
		float rr = rl + stream_region_hex_width;
		float rb = rt + stream_region_hex_height;

		/* window hexagon coords */
		float wr_left   = (rl - tx) * zoom;
		float wr_right  = (rr - tx) * zoom;
		float wr_top    = (rt - ty) * zoom;
		float wr_bottom = (rb - ty) * zoom;
		float wr_left_corner  = (rl - tx + stream_region_hex_width / 4) * zoom;
		float wr_right_corner = (rr - tx - stream_region_hex_width / 4) * zoom;
		float wr_vertical_ctr   = (wr_top + wr_bottom) / 2;
		float wr_horizontal_ctr = (wr_left + wr_right) / 2;
		/* Label the hexagon half height */
		char region_hex_hw_label[64];
		snprintf(region_hex_hw_label, 64, "%ld", lroundf(REGION_UPSCALE * stream_region_hex_size));
		gui_container_pop();
		gui_text(region_hex_hw_label, (struct text_opts) {
			TEXT_OPTS_DEFAULTS,
			.xoffset = wr_horizontal_ctr,
			.yoffset = wr_top,
			.zoffset = 1,
			.size = 20
		});
		gui_container_push(&stack);
		/*
		 * Highlight the region with a white hexagon, from 9 o'clock
		 * going clockwise.
		 */
		gui_line(wr_left, wr_vertical_ctr, 89, 1, 0xffffffff,
		         wr_left_corner, wr_top, 89, 1, 0xffffffff);
		gui_line(wr_left_corner, wr_top, 89, 1, 0xffffffff,
		         wr_right_corner, wr_top, 89, 1, 0xffffffff);
		gui_line(wr_right_corner, wr_top, 89, 1, 0xffffffff,
		         wr_right, wr_vertical_ctr, 89, 1, 0xffffffff);
		gui_line(wr_right, wr_vertical_ctr, 89, 1, 0xffffffff,
		         wr_right_corner, wr_bottom, 89, 1, 0xffffffff);
		gui_line(wr_right_corner, wr_bottom, 89, 1, 0xffffffff,
		         wr_left_corner, wr_bottom, 89, 1, 0xffffffff);
		gui_line(wr_left_corner, wr_bottom, 89, 1, 0xffffffff,
		         wr_left, wr_vertical_ctr, 89, 1, 0xffffffff);
		if (window.unhandled_mouse_press[MOUSEBL]) {
			planet_generation.selected_region_x = wrapidx(rl, planet_generation.composite_size);
			planet_generation.selected_region_y = wrapidx(rt, planet_generation.composite_size);
		}
	}
	gui_container_pop();

	/* Handle after UI has a chance to steal mouse event */
	if (window.unhandled_mouse_press[MOUSEBM]) {
		window.unhandled_mouse_press[MOUSEBM] = 0;
		planet_generation.mouse_captured = 1;
		window_lock_mouse();
	}

	window_submitframe();

	return window.should_close;
}

static int
planet_generation_gl_blit_lithosphere_image(void *_)
{
	struct lithosphere *l = world.lithosphere;
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
	glTextureSubImage2D(planet_generation.lithosphere_img,
	                    0, /* level */
	                    0, 0, /* x,y offset */
	                    LITHOSPHERE_LEN, LITHOSPHERE_LEN, /* w,h */
	                    GL_RGB, GL_UNSIGNED_BYTE,
	                    img);
	free(img);

	return 0;
}

static int
planet_generation_gl_blit_climate_image(void *_)
{
	struct climate *c = world.climate;
	GLubyte *img = xmalloc(CLIMATE_AREA * sizeof(*img) * 3);
	for (size_t i = 0; i < CLIMATE_AREA; ++ i) {
		float temp = 1 - c->inv_temp[i];
		float precip = c->precipitation[i];
		float elev = c->uplift[i];
		enum biome b = biome_class(elev, temp, precip);
		memcpy(&img[i*3], biome_color[b], sizeof(GLubyte)*3);
	}

	glTextureSubImage2D(planet_generation.climate_img,
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
planet_generation_gl_blit_stream_image(void *_)
{
	struct stream_graph *s = world.stream;
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

	glTextureSubImage2D(planet_generation.stream_img,
	                    0, /* level */
	                    0, 0, /* x,y offset */
	                    s->size, s->size, /* w,h */
	                    GL_RGB, GL_UNSIGNED_BYTE,
	                    img);
	free(img);

	return 0;
}

static int
planet_generation_gl_blit_composite_image(void *_)
{
	struct stream_graph *s = world.stream;
	GLubyte *img = xcalloc(s->size * s->size * 3, sizeof(*img));

	/*
	 * NOTE: This is *exactly* the same code we use to blit region height
	 * in worldgen/region.c
	 */
	size_t tri_count = vector_size(s->tris);
	const float cs = planet_generation.composite_size / CLIMATE_LEN;
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
			float temp = 1 - lerp_climate_layer(world.climate->inv_temp, wx, wy, cs);
			float precip = lerp_climate_layer(world.climate->precipitation, wx, wy, cs);
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

	glTextureSubImage2D(planet_generation.composite_img,
	                    0, /* level */
	                    0, 0, /* x,y offset */
	                    s->size, s->size, /* w,h */
	                    GL_RGB, GL_UNSIGNED_BYTE,
	                    img);
	free(img);

	return 0;
}
