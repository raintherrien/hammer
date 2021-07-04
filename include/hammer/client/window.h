#ifndef HAMMER_CLIENT_WINDOW_H_
#define HAMMER_CLIENT_WINDOW_H_

#include "hammer/client/gui/line.h"
#include "hammer/client/gui/map.h"
#include "hammer/client/gui/rect.h"
#include "hammer/client/gui/text.h"
#include <cglm/struct.h>
#include <GL/glew.h>
#include <SDL2/SDL.h>

#define FRAMES_IN_FLIGHT 1
#define FRAME_TIMING_LEN 800

struct window_dims {
	int w;
	int h;
};

struct window_mouse {
	int x;
	int y;
	int mx;
	int my;
};

enum mouse_button {
	MOUSE_BUTTON_LEFT,
	MOUSE_BUTTON_RIGHT,
	MOUSE_BUTTON_MIDDLE,
	MOUSE_BUTTON_COUNT,
};

void window_create(void);
void window_destroy(void);

/* Returns 1 if the window should close, otherwise 0. */
int window_submitframe(void);

void window_lock_mouse(void);
void window_unlock_mouse(void);
void window_capture_mouse(void);
void window_release_mouse(void);
void window_warp_mouse(int x, int y);

int window_mouse_held(enum mouse_button);
int window_mouse_press_peek(enum mouse_button);
int window_mouse_press_take(enum mouse_button);

mat4s window_ortho(void);
float window_aspect(void);
struct window_dims window_dims(void);
struct window_mouse window_mouse(void);
const char *window_text_input(void);

#endif /* HAMMER_CLIENT_WINDOW_H_ */
