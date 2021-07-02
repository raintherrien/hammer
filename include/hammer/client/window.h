#ifndef HAMMER_CLIENT_WINDOW_H_
#define HAMMER_CLIENT_WINDOW_H_

#include "hammer/client/gui/line.h"
#include "hammer/client/gui/map.h"
#include "hammer/client/gui/rect.h"
#include "hammer/client/gui/text.h"
#include <cglm/mat4.h>
#include <GL/glew.h>
#include <SDL2/SDL.h>

#define FRAMES_IN_FLIGHT 1
#define FRAME_TIMING_LEN 800

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

enum {
	MOUSEBL,
	MOUSEBR,
	MOUSEBM,
	MOUSEB_COUNT,
};

#define MAX_TEXT_INPUT_LEN 64

struct window {
	struct gui_text_renderer gui_text_renderer;
	struct gui_map_renderer gui_map_renderer;
	struct gui_rect_renderer gui_rect_renderer;
	struct gui_line_renderer gui_line_renderer;
	gui_container  gui_default_window;
	gui_container *current_container;

	SDL_Window   *handle;
	const Uint8  *keydown;
	SDL_Event    *frame_events;
	SDL_GLContext glcontext;
	struct frame  frames[FRAMES_IN_FLIGHT];
	struct frame_timing timing[FRAME_TIMING_LEN];
	struct frame *current_frame;
	size_t        current_frame_id;
	size_t        text_input_len;
	mat4          ortho_matrix;
	SDL_Keymod    keymod;
	int           resized;
	int           motion_x, motion_y;
	int           mouse_x, mouse_y;
	int           scroll;
	int           width, height;
	int           mouse_held[MOUSEB_COUNT];
	int           unhandled_mouse_press[MOUSEB_COUNT];
	int           should_close;
	int           display_frame_timing;
	char          text_input[MAX_TEXT_INPUT_LEN];
};

extern struct window window;

void window_create(void);
void window_destroy(void);
void window_submitframe(void);

inline static void
window_lock_mouse(void)
{
	SDL_SetRelativeMouseMode(SDL_TRUE);
}

inline static void
window_unlock_mouse(void)
{
	SDL_SetRelativeMouseMode(SDL_FALSE);
}

inline static void
window_capture_mouse(void)
{
	SDL_SetWindowGrab(window.handle, SDL_TRUE);
}

inline static void
window_release_mouse(void)
{
	SDL_SetWindowGrab(window.handle, SDL_FALSE);
}

inline static void
window_warp_mouse(int x, int y)
{
	SDL_WarpMouseInWindow(window.handle, x, y);
}

#endif /* HAMMER_CLIENT_WINDOW_H_ */
