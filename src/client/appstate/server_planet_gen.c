#include "hammer/client/appstate/transition_table.h"
#include "hammer/client/glthread.h"
#include "hammer/client/gui.h"
#include "hammer/client/window.h"
#include "hammer/error.h"
#include "hammer/math.h"
#include "hammer/mem.h"
#include "hammer/net.h"
#include "hammer/time.h"
#include "hammer/worldgen/planet.h"
#include "hammer/worldgen/climate.h"
#include "hammer/worldgen/region.h"
#include "hammer/worldgen/stream.h"
#include "hammer/worldgen/tectonic.h"
#include "hammer/worldgen/world_opts.h"
#include <float.h>
#include <stdio.h>
#include <string.h>

#define PROGRESS_STR_MAX_LEN 64

static struct {
	dltask task;

	struct world_opts opts;

	struct planet_gen_payload *payload;
	time_ns payload_queried_ns;

	enum planet_gen_stage last_stage;
	enum planet_gen_stage focussed_stage;

	gui_btn_state cancel_btn_state;
	unsigned long stream_size;

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

	long lithosphere_generation;
	long climate_generation;
	long stream_generation;

	char lithosphere_progress_str[PROGRESS_STR_MAX_LEN];
	char climate_progress_str[PROGRESS_STR_MAX_LEN];
	char stream_progress_str[PROGRESS_STR_MAX_LEN];
} server_planet_gen;

static int planet_generation_gl_setup(void *);
static int planet_generation_gl_frame(void *);

static int planet_generation_gl_blit_complete_image(void *);

static void planet_generation_frame_async(DL_TASK_ARGS);


dltask *
appstate_server_planet_gen_entry(void *optsx)
{
	server_planet_gen.task = DL_TASK_INIT(planet_generation_frame_async);

	memcpy(&server_planet_gen.opts, optsx, sizeof(server_planet_gen.opts));

	/* realloced per server response */
	server_planet_gen.payload = xmalloc(sizeof(*server_planet_gen.payload));
	server_planet_gen.payload->stage = PLANET_GEN_STAGE_NONE;
	server_planet_gen.payload_queried_ns = 0;

	server_planet_gen.last_stage = PLANET_GEN_STAGE_NONE;
	server_planet_gen.focussed_stage = PLANET_GEN_STAGE_NONE;
	server_planet_gen.cancel_btn_state = 0;
	server_planet_gen.stream_size = world_opts_stream_graph_size(&server_planet_gen.opts);

	server_planet_gen.map_zoom = server_planet_gen.min_map_zoom;
	server_planet_gen.map_tran_x = 0;
	server_planet_gen.map_tran_y = 0;

	server_planet_gen.selected_region_x = FLT_MAX;
	server_planet_gen.selected_region_y = FLT_MAX;
	server_planet_gen.selected_region_size_mag2 = 0;

	server_planet_gen.mouse_captured = 0;

	server_planet_gen.lithosphere_generation = 0;
	server_planet_gen.climate_generation = 0;
	server_planet_gen.stream_generation = 0;

	/*
	 * We don't know how many lithosphere steps there are since this is
	 * user-configurable, but we do know climate and stream graph steps.
	 */
	snprintf(server_planet_gen.lithosphere_progress_str,
	         PROGRESS_STR_MAX_LEN,
	         "Lithosphere Generation: initializing");
	snprintf(server_planet_gen.climate_progress_str,
	         PROGRESS_STR_MAX_LEN,
	         "Climate Model Generation: 0/%ld",
	         (long)CLIMATE_GENERATIONS);
	snprintf(server_planet_gen.stream_progress_str,
	         PROGRESS_STR_MAX_LEN,
	         "Stream Graph Generation: 0/%ld",
	         (long)STREAM_GRAPH_GENERATIONS);

	glthread_execute(planet_generation_gl_setup, NULL);

	return &server_planet_gen.task;
}

void
appstate_server_planet_gen_exit(dltask *_)
{
	(void) _;
	free(server_planet_gen.payload);
}

static void
planet_generation_frame_async(DL_TASK_ARGS)
{
	DL_TASK_ENTRY_VOID;

	if (glthread_execute(planet_generation_gl_frame, NULL) ||
	    server_planet_gen.cancel_btn_state == GUI_BTN_RELEASED)
	{
		client_write(NETMSG_TYPE_CLIENT_REQUEST_SERVER_SHUTDOWN, NULL, 0);
		local_connection_stop();
		transition(&client_appstate_mgr, CLIENT_APPSTATE_TRANSITION_MAIN_MENU_OPEN, NULL);
		return;
	}

	/* Continue requesting payloads until we get the composite */
	if (server_planet_gen.payload->stage != PLANET_GEN_STAGE_COMPOSITE &&
	    server_planet_gen.payload_queried_ns == 0)
	{
		xpinfo("Querying planet generation stage from server");
		server_planet_gen.payload_queried_ns = now_ns();
		client_write(NETMSG_TYPE_CLIENT_QUERY_PLANET_GENERATION_STAGE, NULL, 0);
	}

	/* Respond to any messages; TODO: Reading payload should be separate task */
	enum netmsg_type type;
	size_t sz;
	if (client_peek(&type, &sz)) {
		switch (type) {
		/* The server sent us a payload. Update our view */
		case NETMSG_TYPE_SERVER_RESPONSE_PLANET_GENERATION_PAYLOAD: {
			time_ns response_ns = now_ns() - server_planet_gen.payload_queried_ns;
			server_planet_gen.payload_queried_ns = 0;
			xpinfova("Received generation payload of %zu bytes from server after %llums", sz, response_ns / 1000000);
			server_planet_gen.payload = xrealloc(server_planet_gen.payload, sz);
			client_read(server_planet_gen.payload, sz);
			/* Blit the image */
			glthread_execute(planet_generation_gl_blit_complete_image, NULL);
			/* Switch to new stage when available */
			if (server_planet_gen.last_stage != server_planet_gen.payload->stage) {
				server_planet_gen.focussed_stage = server_planet_gen.payload->stage;
			}
			server_planet_gen.last_stage = server_planet_gen.payload->stage;

			switch (server_planet_gen.last_stage) {
			case PLANET_GEN_STAGE_LITHOSPHERE:
				++ server_planet_gen.lithosphere_generation;
				snprintf(server_planet_gen.lithosphere_progress_str,
				         PROGRESS_STR_MAX_LEN,
				         "Lithosphere Generation: %ld/%ld",
				         server_planet_gen.lithosphere_generation,
				         (long)server_planet_gen.opts.tectonic.generations *
				               server_planet_gen.opts.tectonic.generation_steps);
				break;

			case PLANET_GEN_STAGE_CLIMATE:
				++ server_planet_gen.climate_generation;
				snprintf(server_planet_gen.climate_progress_str,
				         PROGRESS_STR_MAX_LEN,
				         "Climate Model Generation: %ld/%ld",
				         server_planet_gen.climate_generation,
				         (long)CLIMATE_GENERATIONS);
				break;

			case PLANET_GEN_STAGE_STREAM:
				++ server_planet_gen.stream_generation;
				snprintf(server_planet_gen.stream_progress_str,
				         PROGRESS_STR_MAX_LEN,
				         "Stream Graph Generation: %ld/%ld",
				         server_planet_gen.stream_generation,
				         (long)STREAM_GRAPH_GENERATIONS);
				break;

			default: break;
			}
			break;
		}

		default:
			errno = EINVAL;
			xperrorva("unexpected netmsg type: %d", type);
			client_discard(sz);
			break;
		}
	}

	/* Kick off local region generation
	if (server_planet_gen.selected_region_x != FLT_MAX &&
	    server_planet_gen.selected_region_y != FLT_MAX)
	{
		server.world.region_stream_coord_left = server_planet_gen.selected_region_x;
		server.world.region_stream_coord_top  = server_planet_gen.selected_region_y;
		server.world.region_size_mag2 = server_planet_gen.selected_region_size_mag2;
		appstate_transition(APPSTATE_TRANSITION_SERVER_REGION_GEN);
		return;
	}
	*/
}

struct spg_map_tex_params {
	GLuint *texture_ptr;
	GLsizei width_height;
};

static int
planet_generation_gl_setup(void *_)
{
	struct window_dims dims = window_dims();

	server_planet_gen.min_map_zoom = MIN((float)dims.w / LITHOSPHERE_LEN,
	                                     (float)dims.h / LITHOSPHERE_LEN);

	glClearColor(49 / 255.0f, 59 / 255.0f, 58 / 255.0f, 1);

	struct spg_map_tex_params map_tex_params[4] = {
		{ &server_planet_gen.lithosphere_img, LITHOSPHERE_LEN },
		{ &server_planet_gen.climate_img,     CLIMATE_LEN },
		{ &server_planet_gen.stream_img,      server_planet_gen.stream_size },
		{ &server_planet_gen.composite_img,   server_planet_gen.stream_size }
	};

	for (size_t i = 0; i < 4; ++ i) {
		struct spg_map_tex_params t = map_tex_params[i];
		glGenTextures(1, t.texture_ptr);
		glBindTexture(GL_TEXTURE_2D, *t.texture_ptr);
		glTexImage2D(GL_TEXTURE_2D, 0, /* level */
		             GL_RGB8,
		             t.width_height, t.width_height,
		             0, /* border */
		             GL_RGB,
		             GL_UNSIGNED_BYTE,
		             NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glGenerateMipmap(GL_TEXTURE_2D);
	}

	return 0;
}

static int
planet_generation_gl_frame(void *_)
{
	struct window_dims dims = window_dims();
	struct window_mouse mouse = window_mouse();

	server_planet_gen.min_map_zoom = MIN((float)dims.w / LITHOSPHERE_LEN,
	                                     (float)dims.h / LITHOSPHERE_LEN);
	server_planet_gen.map_zoom = MAX(server_planet_gen.min_map_zoom, server_planet_gen.map_zoom);

#if 0 // xxx
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
		float cropx = (dims.w * delta) / d;
		float cropy = (dims.h * delta) / d;
		server_planet_gen.map_tran_x += cropx / 2  / 1024.0f;
		server_planet_gen.map_tran_y += cropy / 2 / 1024.0f;
		server_planet_gen.map_zoom += delta;
	}
#endif

	if (server_planet_gen.mouse_captured) {
		if (!window_mouse_held(MOUSE_BUTTON_MIDDLE)) {
			server_planet_gen.mouse_captured = 0;
			window_unlock_mouse();
		}
		server_planet_gen.map_tran_x -= mouse.mx / (float)dims.w / server_planet_gen.map_zoom;
		server_planet_gen.map_tran_y -= mouse.my / (float)dims.h / server_planet_gen.map_zoom;
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

	const struct map_opts map_opts = {
		MAP_OPTS_DEFAULTS,
		.zoffset = 10, /* background */
		.width  = dims.w,
		.height = dims.h,
		.scale_x = server_planet_gen.map_zoom * (LITHOSPHERE_LEN / (float)dims.w),
		.scale_y = server_planet_gen.map_zoom * (LITHOSPHERE_LEN / (float)dims.h),
		.tran_x = server_planet_gen.map_tran_x,
		.tran_y = server_planet_gen.map_tran_y
	};

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
	snprintf(seed_label, 64, "Seed: %llu", server_planet_gen.opts.seed);
	gui_text(seed_label, normal_text_opts);
	gui_stack_break(&stack);
	gui_text(server_planet_gen.lithosphere_progress_str, normal_text_opts);
	gui_stack_break(&stack);
	gui_text(server_planet_gen.climate_progress_str, normal_text_opts);
	gui_stack_break(&stack);
	gui_text(server_planet_gen.stream_progress_str, normal_text_opts);
	gui_text(" Pause", normal_text_opts);
	gui_stack_break(&stack);
	server_planet_gen.cancel_btn_state = gui_btn(server_planet_gen.cancel_btn_state, "Cancel", btn_opts);
	gui_stack_break(&stack);
	gui_container_pop();

	if (server_planet_gen.focussed_stage == PLANET_GEN_STAGE_COMPOSITE) {
		gui_map(server_planet_gen.composite_img, map_opts);
	} else if (server_planet_gen.last_stage >= PLANET_GEN_STAGE_COMPOSITE) {
		const struct map_opts tab_opts = {
			MAP_OPTS_DEFAULTS,
			.xoffset = dims.w - border_padding - 256,
			.yoffset = border_padding,
			.width  = 64,
			.height = 64,
			.zoffset = 0.5f
		};
		gui_map(server_planet_gen.composite_img, tab_opts);

		if (window_mouse_press_take(MOUSE_BUTTON_LEFT) &&
		    mouse.x >= tab_opts.xoffset &&
		    mouse.x <  tab_opts.xoffset + tab_opts.width &&
		    mouse.y >= tab_opts.yoffset &&
		    mouse.y <  tab_opts.yoffset + tab_opts.height)
		{
			server_planet_gen.focussed_stage = PLANET_GEN_STAGE_COMPOSITE;
		}
	}

	if (server_planet_gen.focussed_stage == PLANET_GEN_STAGE_STREAM) {
		gui_map(server_planet_gen.stream_img, map_opts);
	} else if (server_planet_gen.last_stage >= PLANET_GEN_STAGE_STREAM) {
		const struct map_opts tab_opts = {
			MAP_OPTS_DEFAULTS,
			.xoffset = dims.w - border_padding - 192,
			.yoffset = border_padding,
			.width  = 64,
			.height = 64,
			.zoffset = 0.5f
		};
		gui_map(server_planet_gen.stream_img, tab_opts);

		if (window_mouse_press_take(MOUSE_BUTTON_LEFT) &&
		    mouse.x >= tab_opts.xoffset &&
		    mouse.x <  tab_opts.xoffset + tab_opts.width &&
		    mouse.y >= tab_opts.yoffset &&
		    mouse.y <  tab_opts.yoffset + tab_opts.height)
		{
			server_planet_gen.focussed_stage = PLANET_GEN_STAGE_STREAM;
		}
	}

	if (server_planet_gen.focussed_stage == PLANET_GEN_STAGE_CLIMATE) {
		gui_map(server_planet_gen.climate_img, map_opts);
	} else if (server_planet_gen.last_stage >= PLANET_GEN_STAGE_CLIMATE) {
		const struct map_opts tab_opts = {
			MAP_OPTS_DEFAULTS,
			.xoffset = dims.w - border_padding - 128,
			.yoffset = border_padding,
			.width  = 64,
			.height = 64,
			.zoffset = 0.5f
		};
		gui_map(server_planet_gen.climate_img, tab_opts);

		if (window_mouse_press_take(MOUSE_BUTTON_LEFT) &&
		    mouse.x >= tab_opts.xoffset &&
		    mouse.x <  tab_opts.xoffset + tab_opts.width &&
		    mouse.y >= tab_opts.yoffset &&
		    mouse.y <  tab_opts.yoffset + tab_opts.height)
		{
			server_planet_gen.focussed_stage = PLANET_GEN_STAGE_CLIMATE;
		}
	}

	if (server_planet_gen.focussed_stage == PLANET_GEN_STAGE_LITHOSPHERE) {
		gui_map(server_planet_gen.lithosphere_img, map_opts);
	} else if (server_planet_gen.last_stage >= PLANET_GEN_STAGE_LITHOSPHERE) {
		const struct map_opts tab_opts = {
			MAP_OPTS_DEFAULTS,
			.xoffset = dims.w - border_padding - 64,
			.yoffset = border_padding,
			.width  = 64,
			.height = 64,
			.zoffset = 0.5f
		};
		gui_map(server_planet_gen.lithosphere_img, tab_opts);

		if (window_mouse_press_take(MOUSE_BUTTON_LEFT) &&
		    mouse.x >= tab_opts.xoffset &&
		    mouse.x <  tab_opts.xoffset + tab_opts.width &&
		    mouse.y >= tab_opts.yoffset &&
		    mouse.y <  tab_opts.yoffset + tab_opts.height)
		{
			server_planet_gen.focussed_stage = PLANET_GEN_STAGE_LITHOSPHERE;
		}
	}

	/*
	 * Show the user details about a composite region and perform region
	 * selection.
	 */
	gui_container_push(&stack);
	if (server_planet_gen.focussed_stage == PLANET_GEN_STAGE_COMPOSITE &&
	    !server_planet_gen.mouse_captured)
	{
		gui_text("Left click to select region", normal_text_opts);
		gui_stack_break(&stack);
		gui_text("Ctrl+scroll to resize region", normal_text_opts);
		gui_stack_break(&stack);
		/* Determine the region highlighted */
		float stream_region_size = STREAM_REGION_SIZE_MIN * (1 << server_planet_gen.selected_region_size_mag2);
		float scale = LITHOSPHERE_LEN / (float)server_planet_gen.stream_size;
		float zoom = server_planet_gen.map_zoom * scale;
		float tx = server_planet_gen.map_tran_x * server_planet_gen.stream_size;
		float ty = server_planet_gen.map_tran_y * server_planet_gen.stream_size;
		/* world coords */
		float rl = mouse.x / zoom + tx - stream_region_size / 2;
		float rt = mouse.y / zoom + ty - stream_region_size / 2;
		float rr = rl + stream_region_size;
		float rb = rt + stream_region_size;

		/* window coords */
		float wr_left   = (rl - tx) * zoom;
		float wr_right  = (rr - tx) * zoom;
		float wr_top    = (rt - ty) * zoom;
		float wr_bottom = (rb - ty) * zoom;
		/* Label the region length */
		char region_size_label[64];
		snprintf(region_size_label, 64, "%ld^2", lroundf(REGION_UPSCALE * stream_region_size));
		gui_container_pop();
		gui_text(region_size_label, (struct text_opts) {
			TEXT_OPTS_DEFAULTS,
			.xoffset = wr_left,
			.yoffset = wr_top,
			.zoffset = 1,
			.size = 20
		});
		gui_container_push(&stack);
		/* Highlight the region with a white box */
		gui_line(wr_left, wr_top, 9, 1, 0xffffffff,
		         wr_right, wr_top, 9, 1, 0xffffffff);
		gui_line(wr_left, wr_bottom, 9, 1, 0xffffffff,
		         wr_right, wr_bottom, 9, 1, 0xffffffff);
		gui_line(wr_left, wr_top, 9, 1, 0xffffffff,
		         wr_left, wr_bottom, 9, 1, 0xffffffff);
		gui_line(wr_right, wr_top, 9, 1, 0xffffffff,
		         wr_right, wr_bottom, 9, 1, 0xffffffff);
		if (window_mouse_press_take(MOUSE_BUTTON_LEFT)) {
			server_planet_gen.selected_region_x = wrapidx(rl, server_planet_gen.stream_size);
			server_planet_gen.selected_region_y = wrapidx(rt, server_planet_gen.stream_size);
		}
	}
	gui_container_pop();

	/* Handle after UI has a chance to steal mouse event */
	if (window_mouse_press_take(MOUSE_BUTTON_MIDDLE)) {
		server_planet_gen.mouse_captured = 1;
		window_lock_mouse();
	}

	return window_submitframe();
}

static int
planet_generation_gl_blit_complete_image(void *_)
{
	GLuint destination;
	switch (server_planet_gen.payload->stage) {
	case PLANET_GEN_STAGE_NONE:
		return 0; /* DO NOT BLIT ANYTHING! */

	case PLANET_GEN_STAGE_LITHOSPHERE:
		destination = server_planet_gen.lithosphere_img;
		break;

	case PLANET_GEN_STAGE_CLIMATE:
		destination = server_planet_gen.climate_img;
		break;

	case PLANET_GEN_STAGE_STREAM:
		destination = server_planet_gen.stream_img;
		break;

	case PLANET_GEN_STAGE_COMPOSITE:
		destination = server_planet_gen.composite_img;
		break;
	}

	glTextureSubImage2D(destination,
	                    0, /* level */
	                    0, 0, /* x,y offset */
	                    server_planet_gen.payload->render_size,
	                    server_planet_gen.payload->render_size,
	                    GL_RGB, GL_UNSIGNED_BYTE,
	                    server_planet_gen.payload->render);

	return 0;
}
