#include "hammer/client/window.h"
#include "hammer/error.h"
#include "hammer/time.h"
#include "hammer/vector.h"
#include <errno.h>
#include <cglm/cam.h>
#include <cglm/struct.h>

#define MAX_TEXT_INPUT_LEN 64

struct frame {
	struct gui_text_frame gui_text_frame;
	struct gui_rect_frame gui_rect_frame;
	struct gui_line_frame gui_line_frame;
	unsigned long long frame_begin_ns;
	unsigned long long frame_end_ns;
	GLsync fence;
	size_t id;
};

struct frame_timing {
	uint64_t cpu_ns;
};

struct window_s {
	struct gui_text_renderer gui_text_renderer;
	struct gui_map_renderer gui_map_renderer;
	struct gui_rect_renderer gui_rect_renderer;
	struct gui_line_renderer gui_line_renderer;
	gui_container  gui_default_window;

	SDL_Window   *handle;
	const Uint8  *keydown;
	SDL_Event    *frame_events;
	SDL_GLContext glcontext;
	struct frame  frames[FRAMES_IN_FLIGHT];
	struct frame_timing timing[FRAME_TIMING_LEN];
	struct frame *current_frame;
	size_t        current_frame_id;
	size_t        text_input_len;
	mat4s         ortho_matrix;
	SDL_Keymod    keymod;
	int           resized;
	int           motion_x, motion_y;
	int           mouse_x, mouse_y;
	int           scroll;
	int           width, height;
	int           mouse_held[MOUSE_BUTTON_COUNT];
	int           unhandled_mouse_press[MOUSE_BUTTON_COUNT];
	int           should_close;
	int           display_frame_timing;
	char          text_input[MAX_TEXT_INPUT_LEN];
};

struct window_s window;

static void window_draw_frame_timing(void);
static void window_acquire_next_frame(void);
static void print_GLenum(GLenum e);
static void gl_debug_callback(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar *, const void *);
static void opengl_init(void);

void
window_create(void)
{
	int r = SDL_Init(SDL_INIT_VIDEO);
	if (r) {
		errno = r;
		xpanicva("Error initializing SDL %s", SDL_GetError());
	}

	Uint32 flags  = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
	window.width  = 800;
	window.height = 600;
	window.handle = SDL_CreateWindow("Hammer",
	                                 SDL_WINDOWPOS_UNDEFINED,
	                                 SDL_WINDOWPOS_UNDEFINED,
	                                 window.width,
	                                 window.height,
	                                 flags);
	if (!window.handle) {
		errno = -1;
		xpanicva("Error creating SDL window %s", SDL_GetError());
	}

	/* OpenGL 4.3 core profile */
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	/* 4x MSAA */
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

	window.glcontext = SDL_GL_CreateContext(window.handle);
	if (!window.glcontext) {
		errno = -1;
		xpanicva("Error creating OpenGL context %s", SDL_GetError());
	}

	if (SDL_GL_SetSwapInterval(-1) == -1) {
		errno = -1;
		xperrorva("Error adaptive vsync unsupported %s", SDL_GetError());
	}

	glewExperimental = GL_TRUE;
	GLenum glewrcode = glewInit();
	if (glewrcode != GLEW_OK) {
		errno = glewrcode;
		xpanicva("Error initializing GLEW %s", glewGetErrorString(glewrcode));
	}

	opengl_init();
	glViewport(0, 0, window.width, window.height);

	window.keydown = SDL_GetKeyboardState(NULL);

	gui_text_renderer_create(&window.gui_text_renderer);
	gui_map_renderer_create(&window.gui_map_renderer);
	gui_rect_renderer_create(&window.gui_rect_renderer);
	gui_line_renderer_create(&window.gui_line_renderer);

	for (size_t i = 0; i < FRAMES_IN_FLIGHT; ++ i) {
		gui_text_frame_create(&window.gui_text_renderer,
		                      &window.frames[i].gui_text_frame);
		gui_rect_frame_create(&window.gui_rect_renderer,
		                      &window.frames[i].gui_rect_frame);
		gui_line_frame_create(&window.gui_line_renderer,
		                      &window.frames[i].gui_line_frame);
		window.frames[i].fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		window.frames[i].id = i;
	}
	memset(window.timing, 0, FRAME_TIMING_LEN * sizeof(*window.timing));
	window.current_frame_id = FRAMES_IN_FLIGHT;
	window.current_frame = window.frames;
	gui_window_init(&window.gui_default_window, (struct window_opts) { WINDOW_OPTS_DEFAULT });
	gui_current_container = &window.gui_default_window;
	window.should_close = 0;
	window.display_frame_timing = 0;
}

void
window_destroy(void)
{
	for (size_t i = 0; i < FRAMES_IN_FLIGHT; ++ i) {
		glClientWaitSync(window.frames[i].fence,
		                 GL_SYNC_FLUSH_COMMANDS_BIT,
		                 (GLuint64)-1);
		gui_line_frame_destroy(&window.frames[i].gui_line_frame);
		gui_rect_frame_destroy(&window.frames[i].gui_rect_frame);
		gui_text_frame_destroy(&window.frames[i].gui_text_frame);
	}
	gui_line_renderer_destroy(&window.gui_line_renderer);
	gui_rect_renderer_destroy(&window.gui_rect_renderer);
	gui_map_renderer_destroy(&window.gui_map_renderer);
	gui_text_renderer_destroy(&window.gui_text_renderer);
	vector_free(&window.frame_events);
	SDL_GL_DeleteContext(window.glcontext);
	SDL_DestroyWindow(window.handle);
	SDL_Quit();
}

void
window_lock_mouse(void)
{
	SDL_SetRelativeMouseMode(SDL_TRUE);
}

void
window_unlock_mouse(void)
{
	SDL_SetRelativeMouseMode(SDL_FALSE);
}

void
window_capture_mouse(void)
{
	SDL_SetWindowGrab(window.handle, SDL_TRUE);
}

void
window_release_mouse(void)
{
	SDL_SetWindowGrab(window.handle, SDL_FALSE);
}

void
window_warp_mouse(int x, int y)
{
	SDL_WarpMouseInWindow(window.handle, x, y);
}

int
window_mouse_held(enum mouse_button b)
{
	return window.mouse_held[b];
}

int
window_mouse_press_peek(enum mouse_button b)
{
	return window.unhandled_mouse_press[b];
}

int
window_mouse_press_take(enum mouse_button b)
{
	if (window.unhandled_mouse_press[b]) {
		window.unhandled_mouse_press[b] = 0;
		return 1;
	}
	return 0;
}

mat4s
window_ortho(void)
{
	return window.ortho_matrix;
}

float
window_aspect(void)
{
	return window.width / (float)window.height;
}

struct window_dims
window_dims(void)
{
	return (struct window_dims) { window.width, window.height };
}

struct window_mouse
window_mouse(void)
{
	return (struct window_mouse) { window.mouse_x, window.mouse_y };
}

const char *
window_text_input(void)
{
	return window.text_input;
}

void
gui_map(GLuint texture, struct map_opts opts)
{
	float container_offset[3];
	float w = opts.xoffset + opts.width;
	float h = opts.yoffset + opts.height;
	gui_current_container_get_offsets(container_offset);
	gui_current_container_add_element(w, h);
	opts.xoffset += container_offset[0];
	opts.yoffset += container_offset[1];
	opts.zoffset += container_offset[2];

	struct gui_map_renderer *renderer = &window.gui_map_renderer;

	glUseProgram(renderer->shader);
	glBindVertexArray(renderer->vao);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	glUniform3f(renderer->uniforms.position, opts.xoffset, opts.yoffset, opts.zoffset);
	glUniform2f(renderer->uniforms.dimensions, opts.width, opts.height);
	glUniform2f(renderer->uniforms.translation, opts.tran_x, opts.tran_y);
	glUniform2f(renderer->uniforms.scale, opts.scale_x, opts.scale_y);
	glUniformMatrix4fv(renderer->uniforms.ortho, 1, GL_FALSE, (float *)window.ortho_matrix.raw);
	glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}

float
gui_char_width(float size)
{
	return size * window.gui_text_renderer.font_regular.character_ratio;
}

void
gui_line(float x0, float y0, float z0, float t0, uint32_t c0,
         float x1, float y1, float z1, float t1, uint32_t c1)
{
	struct gui_line_frame *frame = &window.current_frame->gui_line_frame;

	if (GUI_LINE_VBO_SIZE <= frame->vb_vc * sizeof(struct gui_line_vert))
		return;

	frame->vb[frame->vb_vc ++] = (struct gui_line_vert) {
		.position = {x0, y0, z0},
		.thickness = t0,
		.color  = c0
	};
	frame->vb[frame->vb_vc ++] = (struct gui_line_vert) {
		.position = {x1, y1, z1},
		.thickness = t1,
		.color  = c1
	};
}

void
gui_rect(struct rect_opts opts)
{
	float container_offset[3];
	float w = opts.xoffset + opts.width;
	float h = opts.yoffset + opts.height;
	gui_current_container_get_offsets(container_offset);
	gui_current_container_add_element(w, h);
	opts.xoffset += container_offset[0];
	opts.yoffset += container_offset[1];
	opts.zoffset += container_offset[2];

	struct gui_rect_frame *frame = &window.current_frame->gui_rect_frame;

	const size_t VS = sizeof(struct gui_rect_vert);
	float r = ((opts.color & 0xff000000) >> 24) / 255.0f;
	float g = ((opts.color & 0x00ff0000) >> 16) / 255.0f;
	float b = ((opts.color & 0x0000ff00) >>  8) / 255.0f;
	float a = ((opts.color & 0x000000ff) >>  0) / 255.0f;
	float x0 = opts.xoffset;
	float x1 = opts.xoffset + opts.width;
	float y0 = opts.yoffset;
	float y1 = opts.yoffset + opts.height;
	float z = opts.zoffset;

	if ((frame->vb_vc + 6) * VS >= GUI_RECT_VBO_SIZE)
		return;

	struct gui_rect_vert vs[6] = {
		{ .position = {x0, y0, z}, .color = {r, g, b, a} },
		{ .position = {x1, y1, z}, .color = {r, g, b, a} },
		{ .position = {x1, y0, z}, .color = {r, g, b, a} },

		{ .position = {x0, y1, z}, .color = {r, g, b, a} },
		{ .position = {x1, y1, z}, .color = {r, g, b, a} },
		{ .position = {x0, y0, z}, .color = {r, g, b, a} }
	};
	size_t vi = frame->vb_vc;
	memcpy(frame->vb + vi, vs, VS * 6);
	frame->vb_vc += 6;
}

void
gui_text(const char      *text,
         struct text_opts opts)
{
	struct gui_text_renderer *renderer = &window.gui_text_renderer;
	struct gui_text_frame *frame = &window.current_frame->gui_text_frame;
	float char_width = opts.size * renderer->font_regular.character_ratio;
	size_t text_len = strlen(text);

	float container_offset[3];
	float w = text_len * char_width;
	gui_current_container_get_offsets(container_offset);
	gui_current_container_add_element(w, opts.size);
	opts.xoffset += container_offset[0];
	opts.yoffset += container_offset[1];
	opts.zoffset += container_offset[2];

	for (size_t i = 0; i < text_len; ++ i) {
		if (GUI_TEXT_VBO_SIZE <= frame->vb_vc * sizeof(struct gui_text_vert))
			continue;

		int c = (int)text[i] - '!';
		if (c < 0 || c >= GUI_FONT_ATLAS_LAYERS)
			continue;
		float x = opts.xoffset + char_width * i;
		float y = opts.yoffset;
		size_t vi = frame->vb_vc ++;
		frame->vb[vi] = (struct gui_text_vert) {
			.position = {x, y, opts.zoffset},
			.dimensions = {char_width, opts.size},
			.color  = opts.color,
			.weight = opts.weight,
			.style  = opts.style,
			.character = c
		};
	}
}

int
window_submitframe(void)
{
	gui_line_render(&window.gui_line_renderer,
	                &window.current_frame->gui_line_frame);
	gui_rect_render(&window.gui_rect_renderer,
	                &window.current_frame->gui_rect_frame);
	gui_text_render(&window.gui_text_renderer,
	                &window.current_frame->gui_text_frame);

	/* Create fence to complete when this frame is rendered */
	SDL_GL_SwapWindow(window.handle);
	window.current_frame->fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	/* Conclude timing this frame */
	window.current_frame->frame_end_ns = now_ns();

	window_acquire_next_frame();

	return window.should_close;
}

static void
window_draw_frame_timing(void)
{
	const float z = 1; /* above everything */
	const float graph_height_px = 100;
	/* Determine longest frame and scale graph */
	float avg_cpu_ns = 0;
	float max_cpu_ns = 0;
	for (int x = 0; x < FRAME_TIMING_LEN; ++ x) {
		if (window.timing[x].cpu_ns > max_cpu_ns)
			max_cpu_ns = window.timing[x].cpu_ns;
		avg_cpu_ns += window.timing[x].cpu_ns;
	}
	avg_cpu_ns /= FRAME_TIMING_LEN;

	float ys = graph_height_px / max_cpu_ns;
	/* Draw frame graph from right to left to right, right most recent */
	for (int x = 0; x < FRAME_TIMING_LEN; ++ x) {
		size_t tid = (window.current_frame_id + x - FRAME_TIMING_LEN) % FRAME_TIMING_LEN;
		struct frame_timing *t = &window.timing[tid];
		float total_y = graph_height_px - t->cpu_ns * ys;
		gui_line(x, graph_height_px, z, 1, 0xffffffff, x, total_y, 1, 1, 0xffffffff);
	}
	/* Draw y axis label */
	float avgy = graph_height_px - avg_cpu_ns * ys;
	char label[32];
	/* Median */
	gui_line(0, avgy, z, 1, 0xff0000ff, FRAME_TIMING_LEN, avgy, z, 1, 0xffff00ff);
	snprintf(label, 32, "%ldms avg", lroundf(avg_cpu_ns / 1000000));
	gui_text(label, (struct text_opts) {
		TEXT_OPTS_DEFAULTS,
		.xoffset = FRAME_TIMING_LEN + 1,
		.yoffset = avgy - 24 / 2,
		.zoffset = z,
		.size = 24,
		.color = 0xffff00ff
	});
	/* Max */
	gui_line(0, 1, z, 1, 0xff0000ff, FRAME_TIMING_LEN, 1, z, 1, 0xff0000ff);
	snprintf(label, 32, "%ldms max", lroundf(max_cpu_ns / 1000000));
	gui_text(label, (struct text_opts) {
		TEXT_OPTS_DEFAULTS,
		.xoffset = FRAME_TIMING_LEN + 1,
		.yoffset = 1,
		.zoffset = z,
		.size = 24,
		.color = 0xff0000ff
	});
}

static void
window_acquire_next_frame(void)
{
	++ window.current_frame_id;
	size_t new_frame = window.current_frame_id % FRAMES_IN_FLIGHT;
	size_t timing_id = window.current_frame_id % FRAME_TIMING_LEN;
	window.current_frame = &window.frames[new_frame];

	/* Wait for next frame in ring buffer to complete */
	GLsync recycling_fence = window.current_frame->fence;
	GLenum status = GL_TIMEOUT_EXPIRED;
	while (status == GL_TIMEOUT_EXPIRED) {
		status = glClientWaitSync(recycling_fence,
					  GL_SYNC_FLUSH_COMMANDS_BIT,
					  (GLuint64)-1);
		if (status == GL_WAIT_FAILED)
			xperror("Error waiting on frame fence");
	}
	glDeleteSync(recycling_fence);
	window.current_frame->fence = 0;
	window.timing[timing_id].cpu_ns = window.current_frame->frame_end_ns -
	                                  window.current_frame->frame_begin_ns;
	window.current_frame->frame_begin_ns = now_ns();

	/* Begin this frame */
	window.current_frame->id = new_frame;

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	/*
	 * Conditionally draw timing information for this frame before
	 * starting new timers, will have a lag of FRAMES_IN_FLIGHT.
	 */
	if (window.display_frame_timing)
		window_draw_frame_timing();

	window.resized = 0;
	window.motion_x = 0;
	window.motion_y = 0;
	window.scroll = 0;
	window.text_input_len = 0;
	window.unhandled_mouse_press[MOUSE_BUTTON_LEFT] = 0;
	window.unhandled_mouse_press[MOUSE_BUTTON_RIGHT] = 0;
	window.unhandled_mouse_press[MOUSE_BUTTON_MIDDLE] = 0;
	memset(window.text_input, 0, MAX_TEXT_INPUT_LEN * sizeof(*window.text_input));

	vector_clear(&window.frame_events);

	SDL_Event e;
	while (SDL_PollEvent(&e)) {
		vector_push(&window.frame_events, e);
		switch (e.type) {
		case SDL_KEYDOWN:
			if (window.text_input_len == MAX_TEXT_INPUT_LEN - 1)
				break;
			switch (e.key.keysym.scancode) {
			case SDL_SCANCODE_F3:
				window.display_frame_timing = !window.display_frame_timing;
				break;
			case SDL_SCANCODE_RETURN:
				window.text_input[window.text_input_len ++] = '\n';
				break;
			case SDL_SCANCODE_BACKSPACE:
				window.text_input[window.text_input_len ++] = '\b';
				break;
			default:
				break;
			/* case SDL_SCANCODE_ESCAPE: */
			/* case SDL_SCANCODE_TAB: */
			}
			break;
		case SDL_MOUSEBUTTONDOWN:
			if (e.button.button == SDL_BUTTON_LEFT)
				window.unhandled_mouse_press[MOUSE_BUTTON_LEFT] = 1;
			if (e.button.button == SDL_BUTTON_RIGHT)
				window.unhandled_mouse_press[MOUSE_BUTTON_RIGHT] = 1;
			if (e.button.button == SDL_BUTTON_MIDDLE)
				window.unhandled_mouse_press[MOUSE_BUTTON_MIDDLE] = 1;
			break;
		case SDL_MOUSEMOTION:
			window.motion_x += e.motion.xrel;
			window.motion_y += e.motion.yrel;
			break;
		case SDL_MOUSEWHEEL:
			window.scroll += e.wheel.y;
			break;
		case SDL_TEXTINPUT:
			strncpy(window.text_input + window.text_input_len,
			        e.text.text,
			        MAX_TEXT_INPUT_LEN-window.text_input_len-1);
			window.text_input_len = strlen(window.text_input);
			break;
		case SDL_QUIT:
			window.should_close = 1;
			break;
		case SDL_WINDOWEVENT:
			if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
				window.resized = 1;
				window.width  = e.window.data1;
				window.height = e.window.data2;
				glViewport(0, 0, window.width, window.height);
			}
			break;
		}
	}

	/*
	 * TODO: Returns {0,0}, until a mouse event occurs.
	 * We should throw away these values until that happens.
	 */
	Uint32 ms = SDL_GetMouseState(&window.mouse_x, &window.mouse_y);
	window.keymod = SDL_GetModState();

	window.mouse_held[MOUSE_BUTTON_LEFT] = ms & SDL_BUTTON(SDL_BUTTON_LEFT);
	window.mouse_held[MOUSE_BUTTON_RIGHT] = ms & SDL_BUTTON(SDL_BUTTON_RIGHT);
	window.mouse_held[MOUSE_BUTTON_MIDDLE] = ms & SDL_BUTTON(SDL_BUTTON_MIDDLE);

	/*
	 * Orthographic coordinates use top left 0,0.
	 * glm_ortho *negates* near and far, however we want to only render
	 * GUI elements *behind* the camera (positive Z).
	 */
	glm_ortho(0, window.width, window.height, 0, -1, -100, window.ortho_matrix.raw);
}

#ifdef HAMMER_DEBUG_OPENGL
static void
print_GLenum(GLenum e)
{
#define PRINT_ENUM_CASE(e) case e: fprintf(stderr, "[OPENGL] \t" #e "\n"); break
	switch (e) {
	PRINT_ENUM_CASE(GL_INVALID_ENUM);
	PRINT_ENUM_CASE(GL_INVALID_VALUE);
	PRINT_ENUM_CASE(GL_INVALID_OPERATION);
	PRINT_ENUM_CASE(GL_STACK_OVERFLOW);
	PRINT_ENUM_CASE(GL_STACK_UNDERFLOW);
	PRINT_ENUM_CASE(GL_OUT_OF_MEMORY);
	PRINT_ENUM_CASE(GL_INVALID_FRAMEBUFFER_OPERATION);
	PRINT_ENUM_CASE(GL_CONTEXT_LOST);
	PRINT_ENUM_CASE(GL_TABLE_TOO_LARGE);
	PRINT_ENUM_CASE(GL_DEBUG_SEVERITY_HIGH);
	PRINT_ENUM_CASE(GL_DEBUG_SEVERITY_MEDIUM);
	PRINT_ENUM_CASE(GL_DEBUG_SEVERITY_LOW);
	PRINT_ENUM_CASE(GL_DEBUG_SEVERITY_NOTIFICATION);
	PRINT_ENUM_CASE(GL_DEBUG_SOURCE_API);
	PRINT_ENUM_CASE(GL_DEBUG_SOURCE_WINDOW_SYSTEM);
	PRINT_ENUM_CASE(GL_DEBUG_SOURCE_SHADER_COMPILER);
	PRINT_ENUM_CASE(GL_DEBUG_SOURCE_THIRD_PARTY);
	PRINT_ENUM_CASE(GL_DEBUG_SOURCE_APPLICATION);
	PRINT_ENUM_CASE(GL_DEBUG_SOURCE_OTHER);
	PRINT_ENUM_CASE(GL_DEBUG_TYPE_ERROR);
	PRINT_ENUM_CASE(GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR);
	PRINT_ENUM_CASE(GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR);
	PRINT_ENUM_CASE(GL_DEBUG_TYPE_PORTABILITY);
	PRINT_ENUM_CASE(GL_DEBUG_TYPE_PERFORMANCE);
	PRINT_ENUM_CASE(GL_DEBUG_TYPE_MARKER);
	PRINT_ENUM_CASE(GL_DEBUG_TYPE_PUSH_GROUP);
	PRINT_ENUM_CASE(GL_DEBUG_TYPE_POP_GROUP);
	PRINT_ENUM_CASE(GL_DEBUG_TYPE_OTHER);
	}
#undef PRINT_ENUM_CASE
}

static void
gl_debug_callback(
	GLenum        src,
	GLenum        type,
	GLuint        id,
	GLenum        severity,
	GLsizei       len,
	const GLchar *msg,
	const void   *userParam
) {
	(void) id;
	(void) userParam;
	fprintf(stderr, "[OPENGL] %.*s\n", len, msg);
	print_GLenum(severity);
	print_GLenum(src);
	print_GLenum(type);
}
#endif

static void
opengl_init(void)
{
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

#ifdef HAMMER_DEBUG_OPENGL
	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(gl_debug_callback, NULL);
	/* Report all but GL_DEBUG_SEVERITY_NOTIFICATION */
	glDebugMessageControl(GL_DONT_CARE, /* source */
	                      GL_DONT_CARE, /* type */
	                      GL_DONT_CARE, /* severity */
	                      0,            /* message id count */
	                      NULL,         /* message ids ptr */
	                      GL_TRUE);     /* enabled */
	glDebugMessageControl(GL_DONT_CARE,
	                      GL_DONT_CARE,
	                      GL_DEBUG_SEVERITY_NOTIFICATION,
	                      0,
	                      NULL,
	                      GL_FALSE);
#endif
}
