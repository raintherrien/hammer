#ifndef HAMMER_WINDOW_H_
#define HAMMER_WINDOW_H_

#include "hammer/gui/img.h"
#include "hammer/gui/rect.h"
#include "hammer/gui/text.h"
#include <cglm/mat4.h>
#include <GL/glew.h>
#include <SDL2/SDL.h>

#define FRAMES_IN_FLIGHT 1

struct frame {
	struct gui_text_frame gui_text_frame;
	struct gui_rect_frame gui_rect_frame;
	GLsync fence;
	size_t id;
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
	struct gui_img_renderer gui_img_renderer;
	struct gui_rect_renderer gui_rect_renderer;

	SDL_Window   *handle;
	const Uint8  *keydown;
	SDL_Event    *frame_events;
	SDL_GLContext glcontext;
	struct frame  frames[FRAMES_IN_FLIGHT];
	struct frame *current_frame;
	size_t        current_frame_id;
	size_t        text_input_len;
	mat4          ortho_matrix;
	SDL_Keymod    keymod;
	int           motion_x, motion_y;
	int           mouse_x, mouse_y;
	int           scroll;
	int           width, height;
	int           mouse_held[MOUSEB_COUNT];
	int           mouse_pressed[MOUSEB_COUNT];
	int           should_close;
	char          text_input[MAX_TEXT_INPUT_LEN];
};

extern struct window window;

void window_create(void);
void window_destroy(void);
int  window_startframe(void);
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

#endif /* HAMMER_WINDOW_H_ */
