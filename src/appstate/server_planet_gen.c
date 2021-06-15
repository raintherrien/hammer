#include "hammer/appstate.h"
#include "hammer/glthread.h"
#include "hammer/gui.h"
#include "hammer/mem.h"
#include "hammer/server.h"
#include "hammer/vector.h"
#include "hammer/window.h"
#include "hammer/worldgen/biome.h"
#include "hammer/worldgen/climate.h"
#include "hammer/worldgen/region.h"
#include "hammer/worldgen/stream.h"
#include "hammer/worldgen/tectonic.h"
#include <float.h>
#include <string.h>

/*
 * TODO: All of these updates steps should happen asynchronously. This
 * wouldn't be hard with an atomic GLchar[] or two that the render thread
 * checks and swaps out when the update thread has something to display.
 */

#define PROGRESS_STR_MAX_LEN 64
#define BIOME_TOOLTIP_STR_MAX_LEN 64

dltask appstate_server_planet_gen_frame;

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
} server_planet_gen;

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
appstate_server_planet_gen_setup(void)
{
	appstate_server_planet_gen_frame = DL_TASK_INIT(planet_generation_frame_async);

	server.planet.lithosphere = xmalloc(sizeof(*server.planet.lithosphere));
	server.planet.climate = xmalloc(sizeof(*server.planet.climate));
	server.planet.stream = xmalloc(sizeof(*server.planet.stream));
	lithosphere_create(server.planet.lithosphere, &server.world.opts);

	server_planet_gen.current_stage = PLANET_STAGE_LITHOSPHERE;
	server_planet_gen.focussed_stage = PLANET_STAGE_LITHOSPHERE;
	server_planet_gen.cancel_btn_state = 0;
	server_planet_gen.composite_size = 1 << (9 + server.world.opts.scale);

	server_planet_gen.min_map_zoom = MIN((float)window.width / LITHOSPHERE_LEN,
	                                     (float)window.height / LITHOSPHERE_LEN);
	server_planet_gen.map_zoom = server_planet_gen.min_map_zoom;
	server_planet_gen.map_tran_x = 0;
	server_planet_gen.map_tran_y = 0;

	server_planet_gen.selected_region_x = FLT_MAX;
	server_planet_gen.selected_region_y = FLT_MAX;
	server_planet_gen.selected_region_size_mag2 = 0;

	server_planet_gen.mouse_captured = 0;
	server_planet_gen.paused = 0;

	memset(server_planet_gen.lithosphere_progress_str, 0, PROGRESS_STR_MAX_LEN * sizeof(char));
	memset(server_planet_gen.climate_progress_str, 0, PROGRESS_STR_MAX_LEN * sizeof(char));
	memset(server_planet_gen.stream_progress_str, 0, PROGRESS_STR_MAX_LEN * sizeof(char));
	memset(server_planet_gen.biome_tooltip_str, 0, PROGRESS_STR_MAX_LEN * sizeof(char));

	glthread_execute(planet_generation_gl_setup, NULL);
}

void
appstate_server_planet_gen_teardown(void)
{
	/* TODO: Images never freed, mouse never potentially released */

	if (server.planet.stream) {
		if (server_planet_gen.current_stage >= PLANET_STAGE_STREAM)
			stream_graph_destroy(server.planet.stream);
		free(server.planet.stream);
		server.planet.stream = NULL;
	}
	if (server.planet.climate) {
		if (server_planet_gen.current_stage >= PLANET_STAGE_CLIMATE)
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
appstate_server_planet_gen_reset_region_selection(void)
{
	server_planet_gen.selected_region_x = FLT_MAX;
	server_planet_gen.selected_region_y = FLT_MAX;
}

static void
planet_generation_frame_async(DL_TASK_ARGS)
{
	DL_TASK_ENTRY_VOID;

	if (glthread_execute(planet_generation_gl_frame, NULL) ||
	    server_planet_gen.cancel_btn_state == GUI_BTN_RELEASED)
	{
		appstate_transition(APPSTATE_TRANSITION_SERVER_PLANET_GEN_CANCEL);
		return;
	}

	switch (server_planet_gen.current_stage) {
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
	size_t steps = server.world.opts.tectonic.generations *
	                 server.world.opts.tectonic.generation_steps;
	if (server.planet.lithosphere->generation >= steps) {
		/* Kick off climate loop */
		climate_create(server.planet.climate, server.planet.lithosphere);
		server_planet_gen.current_stage = PLANET_STAGE_CLIMATE;
		server_planet_gen.focussed_stage = PLANET_STAGE_CLIMATE;
		return;
	}

	if (!server_planet_gen.paused) {
		lithosphere_update(server.planet.lithosphere, &server.world.opts);
		glthread_execute(planet_generation_gl_blit_lithosphere_image, NULL);
	}
}

static void
planet_generation_climate_frame(void)
{
	if (server.planet.climate->generation >= CLIMATE_GENERATIONS) {
		/* Kick off stream loop */
		stream_graph_create(server.planet.stream,
		                    server.planet.climate,
		                    server.world.opts.seed,
		                    server_planet_gen.composite_size);
		server_planet_gen.current_stage = PLANET_STAGE_STREAM;
		server_planet_gen.focussed_stage = PLANET_STAGE_STREAM;
		return;
	}

	if (!server_planet_gen.paused) {
		climate_update(server.planet.climate);
		glthread_execute(planet_generation_gl_blit_climate_image, NULL);
	}
}

static void
planet_generation_stream_frame(void)
{
	if (server.planet.stream->generation >= STREAM_GRAPH_GENERATIONS) {
		/* Kick off composite loop */
		glthread_execute(planet_generation_gl_blit_composite_image, NULL);
		server_planet_gen.current_stage = PLANET_STAGE_COMPOSITE;
		server_planet_gen.focussed_stage = PLANET_STAGE_COMPOSITE;
		return;
	}

	if (!server_planet_gen.paused) {
		stream_graph_update(server.planet.stream);
		glthread_execute(planet_generation_gl_blit_stream_image, NULL);
	}
}

static void
planet_generation_composite_frame(void)
{
	if (server_planet_gen.selected_region_x != FLT_MAX &&
	    server_planet_gen.selected_region_y != FLT_MAX)
	{
		/* Kick off local region generation */
		server.world.region_stream_coord_left = server_planet_gen.selected_region_x;
		server.world.region_stream_coord_top  = server_planet_gen.selected_region_y;
		server.world.region_size_mag2 = server_planet_gen.selected_region_size_mag2;
		appstate_transition(APPSTATE_TRANSITION_SERVER_REGION_GEN);
		return;
	}
}

static int
planet_generation_gl_setup(void *_)
{
	glClearColor(49 / 255.0f, 59 / 255.0f, 58 / 255.0f, 1);

	glGenTextures(1, &server_planet_gen.lithosphere_img);
	glBindTexture(GL_TEXTURE_2D, server_planet_gen.lithosphere_img);
	glTexImage2D(GL_TEXTURE_2D, 0, /* level */
	                            GL_RGB8,
	                            LITHOSPHERE_LEN, LITHOSPHERE_LEN,
	                            0, /* border */
	                            GL_RGB,
	                            GL_UNSIGNED_BYTE,
	                            NULL);
	glTextureParameteri(server_planet_gen.lithosphere_img, GL_TEXTURE_MAX_LEVEL, 0);
	glTextureParameteri(server_planet_gen.lithosphere_img, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(server_planet_gen.lithosphere_img, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteri(server_planet_gen.lithosphere_img, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTextureParameteri(server_planet_gen.lithosphere_img, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glGenerateTextureMipmap(server_planet_gen.lithosphere_img);

	glGenTextures(1, &server_planet_gen.climate_img);
	glBindTexture(GL_TEXTURE_2D, server_planet_gen.climate_img);
	glTexImage2D(GL_TEXTURE_2D, 0, /* level */
	                            GL_RGB8,
	                            CLIMATE_LEN, CLIMATE_LEN,
	                            0, /* border */
	                            GL_RGB,
	                            GL_UNSIGNED_BYTE,
	                            NULL);
	glTextureParameteri(server_planet_gen.climate_img, GL_TEXTURE_MAX_LEVEL, 0);
	glTextureParameteri(server_planet_gen.climate_img, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(server_planet_gen.climate_img, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteri(server_planet_gen.climate_img, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTextureParameteri(server_planet_gen.climate_img, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glGenerateTextureMipmap(server_planet_gen.climate_img);

	glGenTextures(1, &server_planet_gen.stream_img);
	glBindTexture(GL_TEXTURE_2D, server_planet_gen.stream_img);
	glTexImage2D(GL_TEXTURE_2D, 0, /* level */
	                            GL_RGB8,
	                            server_planet_gen.composite_size, server_planet_gen.composite_size,
	                            0, /* border */
	                            GL_RGB,
	                            GL_UNSIGNED_BYTE,
	                            NULL);
	glTextureParameteri(server_planet_gen.stream_img, GL_TEXTURE_MAX_LEVEL, 0);
	glTextureParameteri(server_planet_gen.stream_img, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(server_planet_gen.stream_img, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteri(server_planet_gen.stream_img, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTextureParameteri(server_planet_gen.stream_img, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glGenerateTextureMipmap(server_planet_gen.stream_img);

	glGenTextures(1, &server_planet_gen.composite_img);
	glBindTexture(GL_TEXTURE_2D, server_planet_gen.composite_img);
	glTexImage2D(GL_TEXTURE_2D, 0, /* level */
	                            GL_RGB8,
	                            server_planet_gen.composite_size, server_planet_gen.composite_size,
	                            0, /* border */
	                            GL_RGB,
	                            GL_UNSIGNED_BYTE,
	                            NULL);
	glTextureParameteri(server_planet_gen.composite_img, GL_TEXTURE_MAX_LEVEL, 0);
	glTextureParameteri(server_planet_gen.composite_img, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(server_planet_gen.composite_img, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteri(server_planet_gen.composite_img, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTextureParameteri(server_planet_gen.composite_img, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glGenerateTextureMipmap(server_planet_gen.composite_img);

	return 0;
}

static int
planet_generation_gl_frame(void *_)
{
	server_planet_gen.min_map_zoom = MIN((float)window.width / LITHOSPHERE_LEN,
	                       (float)window.height / LITHOSPHERE_LEN);
	server_planet_gen.map_zoom = MAX(server_planet_gen.min_map_zoom, server_planet_gen.map_zoom);

	if (window.scroll && (window.keymod & KMOD_LCTRL ||
	                      window.keymod & KMOD_RCTRL))
	{
		server_planet_gen.selected_region_size_mag2 =
			CLAMP(server_planet_gen.selected_region_size_mag2 - signf(window.scroll),
			      0, MIN(server.world.opts.scale, STREAM_REGION_SIZE_MAX_MAG2));
	}
	else if (window.scroll)
	{
		float delta = window.scroll / 10.0f;
		if (server_planet_gen.map_zoom + delta < server_planet_gen.min_map_zoom)
			delta = server_planet_gen.min_map_zoom - server_planet_gen.map_zoom;
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
		float d = server_planet_gen.map_zoom * (server_planet_gen.map_zoom + delta);
		float cropx = (window.width  * delta) / d;
		float cropy = (window.height * delta) / d;
		server_planet_gen.map_tran_x += cropx / 2  / 1024.0f;
		server_planet_gen.map_tran_y += cropy / 2 / 1024.0f;
		server_planet_gen.map_zoom += delta;
	}

	if (server_planet_gen.mouse_captured) {
		if (!window.mouse_held[MOUSEBM]) {
			server_planet_gen.mouse_captured = 0;
			window_unlock_mouse();
		}
		server_planet_gen.map_tran_x -= window.motion_x / (float)window.width  / server_planet_gen.map_zoom;
		server_planet_gen.map_tran_y -= window.motion_y / (float)window.height / server_planet_gen.map_zoom;
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
		.scale_x = server_planet_gen.map_zoom * (LITHOSPHERE_LEN / (float)window.width),
		.scale_y = server_planet_gen.map_zoom * (LITHOSPHERE_LEN / (float)window.height),
		.tran_x = server_planet_gen.map_tran_x,
		.tran_y = server_planet_gen.map_tran_y
	};

	snprintf(server_planet_gen.lithosphere_progress_str,
	         PROGRESS_STR_MAX_LEN,
	         "Lithosphere Generation: %ld/%ld",
	         (long)server.planet.lithosphere->generation,
	         (long)server.world.opts.tectonic.generations *
	               server.world.opts.tectonic.generation_steps);

	snprintf(server_planet_gen.climate_progress_str,
	         PROGRESS_STR_MAX_LEN,
	         "Climate Model Generation: %ld/%ld",
	         (long)(server.planet.climate ? server.planet.climate->generation : 0),
	         (long)CLIMATE_GENERATIONS);

	snprintf(server_planet_gen.stream_progress_str,
	         PROGRESS_STR_MAX_LEN,
	         "Stream Graph Generation: %ld/%ld",
	         (long)(server.planet.stream ? server.planet.stream->generation : 0),
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
	snprintf(seed_label, 64, "Seed: %llu", server.world.opts.seed);
	gui_text(seed_label, normal_text_opts);
	gui_stack_break(&stack);
	gui_text(server_planet_gen.lithosphere_progress_str, normal_text_opts);
	gui_stack_break(&stack);
	gui_text(server_planet_gen.climate_progress_str, normal_text_opts);
	gui_stack_break(&stack);
	gui_text(server_planet_gen.stream_progress_str, normal_text_opts);
	gui_stack_break(&stack);
	server_planet_gen.paused = gui_check(server_planet_gen.paused, check_opts);
	gui_text(" Pause", normal_text_opts);
	gui_stack_break(&stack);
	server_planet_gen.cancel_btn_state = gui_btn(server_planet_gen.cancel_btn_state, "Cancel", btn_opts);
	gui_stack_break(&stack);

	/*
	 * Show the user what biome they're hovering over on the climate image.
	 */
	if (server_planet_gen.focussed_stage == PLANET_STAGE_CLIMATE &&
	    window.mouse_x >= 0 && window.mouse_y >= 0 &&
	    window.mouse_x < window.width && window.mouse_y < window.height)
	{
		size_t imgx = window.mouse_x / server_planet_gen.map_zoom + server_planet_gen.map_tran_x * CLIMATE_LEN;
		size_t imgy = window.mouse_y / server_planet_gen.map_zoom + server_planet_gen.map_tran_y * CLIMATE_LEN;
		size_t i = wrapidx(imgy, CLIMATE_LEN) * CLIMATE_LEN + wrapidx(imgx, CLIMATE_LEN);
		float temp = 1 - server.planet.climate->inv_temp[i];
		float precip = server.planet.climate->precipitation[i];
		float elev = server.planet.climate->uplift[i];
		enum biome b = biome_class(elev, temp, precip);
		snprintf(server_planet_gen.biome_tooltip_str,
		         BIOME_TOOLTIP_STR_MAX_LEN,
		         "Hovering over: %s",
		         biome_name[b]);
		gui_text(server_planet_gen.biome_tooltip_str, normal_text_opts);
		gui_stack_break(&stack);
	}

	gui_container_pop();

	if (server_planet_gen.focussed_stage == PLANET_STAGE_COMPOSITE) {
		gui_map(server_planet_gen.composite_img, map_opts);
	} else if (server_planet_gen.current_stage == PLANET_STAGE_COMPOSITE) {
		const struct map_opts tab_opts = {
			MAP_OPTS_DEFAULTS,
			.xoffset = window.width - border_padding - 256,
			.yoffset = border_padding,
			.width  = 64,
			.height = 64,
			.zoffset = 0.5f
		};
		gui_map(server_planet_gen.composite_img, tab_opts);

		if (window.unhandled_mouse_press[MOUSEBL] &&
		    window.mouse_x >= tab_opts.xoffset &&
		    window.mouse_x <  tab_opts.xoffset + tab_opts.width &&
		    window.mouse_y >= tab_opts.yoffset &&
		    window.mouse_y <  tab_opts.yoffset + tab_opts.height)
		{
			window.unhandled_mouse_press[MOUSEBL] = 0;
			server_planet_gen.focussed_stage = PLANET_STAGE_COMPOSITE;
		}
	}

	if (server_planet_gen.focussed_stage == PLANET_STAGE_STREAM) {
		gui_map(server_planet_gen.stream_img, map_opts);
	} else if (server_planet_gen.current_stage >= PLANET_STAGE_STREAM) {
		const struct map_opts tab_opts = {
			MAP_OPTS_DEFAULTS,
			.xoffset = window.width - border_padding - 192,
			.yoffset = border_padding,
			.width  = 64,
			.height = 64,
			.zoffset = 0.5f
		};
		gui_map(server_planet_gen.stream_img, tab_opts);

		if (window.unhandled_mouse_press[MOUSEBL] &&
		    window.mouse_x >= tab_opts.xoffset &&
		    window.mouse_x <  tab_opts.xoffset + tab_opts.width &&
		    window.mouse_y >= tab_opts.yoffset &&
		    window.mouse_y <  tab_opts.yoffset + tab_opts.height)
		{
			window.unhandled_mouse_press[MOUSEBL] = 0;
			server_planet_gen.focussed_stage = PLANET_STAGE_STREAM;
		}
	}

	if (server_planet_gen.focussed_stage == PLANET_STAGE_CLIMATE) {
		gui_map(server_planet_gen.climate_img, map_opts);
	} else if (server_planet_gen.current_stage >= PLANET_STAGE_CLIMATE) {
		const struct map_opts tab_opts = {
			MAP_OPTS_DEFAULTS,
			.xoffset = window.width - border_padding - 128,
			.yoffset = border_padding,
			.width  = 64,
			.height = 64,
			.zoffset = 0.5f
		};
		gui_map(server_planet_gen.climate_img, tab_opts);

		if (window.unhandled_mouse_press[MOUSEBL] &&
		    window.mouse_x >= tab_opts.xoffset &&
		    window.mouse_x <  tab_opts.xoffset + tab_opts.width &&
		    window.mouse_y >= tab_opts.yoffset &&
		    window.mouse_y <  tab_opts.yoffset + tab_opts.height)
		{
			window.unhandled_mouse_press[MOUSEBL] = 0;
			server_planet_gen.focussed_stage = PLANET_STAGE_CLIMATE;
		}
	}

	if (server_planet_gen.focussed_stage == PLANET_STAGE_LITHOSPHERE) {
		gui_map(server_planet_gen.lithosphere_img, map_opts);
	} else if (server_planet_gen.current_stage >= PLANET_STAGE_LITHOSPHERE) {
		const struct map_opts tab_opts = {
			MAP_OPTS_DEFAULTS,
			.xoffset = window.width - border_padding - 64,
			.yoffset = border_padding,
			.width  = 64,
			.height = 64,
			.zoffset = 0.5f
		};
		gui_map(server_planet_gen.lithosphere_img, tab_opts);

		if (window.unhandled_mouse_press[MOUSEBL] &&
		    window.mouse_x >= tab_opts.xoffset &&
		    window.mouse_x <  tab_opts.xoffset + tab_opts.width &&
		    window.mouse_y >= tab_opts.yoffset &&
		    window.mouse_y <  tab_opts.yoffset + tab_opts.height)
		{
			window.unhandled_mouse_press[MOUSEBL] = 0;
			server_planet_gen.focussed_stage = PLANET_STAGE_LITHOSPHERE;
		}
	}

	/*
	 * Show the user details about a composite region and perform region
	 * selection.
	 */
	gui_container_push(&stack);
	if (server_planet_gen.focussed_stage == PLANET_STAGE_COMPOSITE) {
		gui_text("Left click to select region", normal_text_opts);
		gui_stack_break(&stack);
		gui_text("Ctrl+scroll to resize region", normal_text_opts);
		gui_stack_break(&stack);
		/* Determine the region highlighted */
		float stream_region_hex_size = STREAM_REGION_SIZE_MIN * (1 << server_planet_gen.selected_region_size_mag2);
		float stream_region_hex_width = stream_region_hex_size * 2;
		float stream_region_hex_height = stream_region_hex_size * sqrtf(3);
		float scale = LITHOSPHERE_LEN / (float)server_planet_gen.composite_size;
		float zoom = server_planet_gen.map_zoom * scale;
		float tx = server_planet_gen.map_tran_x * server_planet_gen.composite_size;
		float ty = server_planet_gen.map_tran_y * server_planet_gen.composite_size;
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
			server_planet_gen.selected_region_x = wrapidx(rl, server_planet_gen.composite_size);
			server_planet_gen.selected_region_y = wrapidx(rt, server_planet_gen.composite_size);
		}
	}
	gui_container_pop();

	/* Handle after UI has a chance to steal mouse event */
	if (window.unhandled_mouse_press[MOUSEBM]) {
		window.unhandled_mouse_press[MOUSEBM] = 0;
		server_planet_gen.mouse_captured = 1;
		window_lock_mouse();
	}

	window_submitframe();

	return window.should_close;
}

static int
planet_generation_gl_blit_lithosphere_image(void *_)
{
	struct lithosphere *l = server.planet.lithosphere;
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
	glTextureSubImage2D(server_planet_gen.lithosphere_img,
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
	struct climate *c = server.planet.climate;
	GLubyte *img = xmalloc(CLIMATE_AREA * sizeof(*img) * 3);
	for (size_t i = 0; i < CLIMATE_AREA; ++ i) {
		float temp = 1 - c->inv_temp[i];
		float precip = c->precipitation[i];
		float elev = c->uplift[i];
		enum biome b = biome_class(elev, temp, precip);
		memcpy(&img[i*3], biome_color[b], sizeof(GLubyte)*3);
	}

	glTextureSubImage2D(server_planet_gen.climate_img,
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
	struct stream_graph *s = server.planet.stream;
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

	glTextureSubImage2D(server_planet_gen.stream_img,
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
	struct stream_graph *s = server.planet.stream;
	GLubyte *img = xcalloc(s->size * s->size * 3, sizeof(*img));

	/*
	 * NOTE: This is *exactly* the same code we use to blit region height
	 * in worldgen/region.c
	 */
	size_t tri_count = vector_size(s->tris);
	const float cs = server_planet_gen.composite_size / CLIMATE_LEN;
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

	glTextureSubImage2D(server_planet_gen.composite_img,
	                    0, /* level */
	                    0, 0, /* x,y offset */
	                    s->size, s->size, /* w,h */
	                    GL_RGB, GL_UNSIGNED_BYTE,
	                    img);
	free(img);

	return 0;
}
