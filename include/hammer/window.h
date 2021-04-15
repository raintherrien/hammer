#ifndef HAMMER_WINDOW_H_
#define HAMMER_WINDOW_H_

#include <GL/glew.h>
#include <SDL2/SDL.h>

#define FRAMES_IN_FLIGHT 1

struct frame {
	GLsync fence;
	size_t id;
};

struct window {
	SDL_Window   *handle;
	const Uint8  *keydown;
	SDL_Event    *frame_events;
	SDL_GLContext glcontext;
	struct frame  frames[FRAMES_IN_FLIGHT];
	size_t        current_frame;
	int           motion_x, motion_y;
	int           mouse_x, mouse_y;
	int           width, height;
};

extern struct window window;

void window_create(void);
void window_destroy(void);
int  window_startframe(void);

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
