#ifndef HAMMER_GUI_H_
#define HAMMER_GUI_H_

#include <stddef.h>
#include <stdint.h>
#include <GL/glew.h>

extern void *gui_element_with_focus;

typedef enum {
	GUI_BTN_PRESSED  = 1<<0,
	GUI_BTN_HELD     = 1<<1,
	GUI_BTN_RELEASED = 1<<2
} gui_btn_state;

typedef enum {
	TEXT_STYLE_ITALIC        = 1<<0,
	/* TODO: Other style elements
	TEXT_STYLE_UNDERLINE     = 1<<2,
	TEXT_STYLE_STRIKETHROUGH = 1<<3
	*/
} text_style;

typedef struct {
	float hpadding;
	float vpadding;
	float xoffset;
	float yoffset;
	float zoffset;
	float line_width;
	float line_height;
} gui_stack;

typedef struct {
	float hpadding;
	float vpadding;
	float xoffset;
	float yoffset;
	float zoffset;
} gui_window;

typedef struct gui_container_s {
	struct gui_container_s *parent;
	enum  { GUI_WINDOW, GUI_STACK } type;
	union { gui_window window; gui_stack stack; };
} gui_container;

struct btn_opts {
	uint32_t foreground;
	uint32_t background;
	text_style style;
	float size;
	float weight;
	float xoffset;
	float yoffset;
	float zoffset;
	float width;
	float height;
};

struct check_opts {
	uint32_t foreground;
	uint32_t background;
	text_style style;
	float size;
	float weight;
	float xoffset;
	float yoffset;
	float zoffset;
	float width;
	float height;
};

struct edit_opts {
	uint32_t foreground;
	uint32_t background;
	text_style style;
	float size;
	float weight;
	float xoffset;
	float yoffset;
	float zoffset;
	float width;
};

struct map_opts {
	float xoffset;
	float yoffset;
	float zoffset;
	float width;
	float height;
	float tran_x;
	float tran_y;
	float scale_x;
	float scale_y;
};

struct rect_opts {
	uint32_t color;
	float xoffset;
	float yoffset;
	float zoffset;
	float width;
	float height;
};

struct text_opts {
	uint32_t color;
	text_style style;
	float size;
	float weight;
	float xoffset;
	float yoffset;
	float zoffset;
};

#define BTN_OPTS_DEFAULTS .foreground = 0x000000ff, \
                          .background = 0x9c9c9cff, \
                          .style = 0,               \
                          .weight = 128,            \
                          .xoffset = 0,             \
                          .yoffset = 0,             \
                          .zoffset = 0

#define CHECK_OPTS_DEFAULTS BTN_OPTS_DEFAULTS

#define EDIT_OPTS_DEFAULTS BTN_OPTS_DEFAULTS

#define MAP_OPTS_DEFAULTS .xoffset = 0, \
                          .yoffset = 0, \
                          .zoffset = 0, \
                          .width   = 0, \
                          .height  = 0, \
                          .tran_x  = 0, \
                          .tran_y  = 0, \
                          .scale_x = 1, \
                          .scale_y = 1

#define RECT_OPTS_DEFAULTS .color = 0x9c9c9cff, \
                           .xoffset = 0, \
                           .yoffset = 0, \
                           .zoffset = 0

#define TEXT_OPTS_DEFAULTS .color = 0xffffffff, \
                           .style = 0,          \
                           .size  = 16,         \
                           .weight = 128,       \
                           .xoffset = 0,        \
                           .yoffset = 0,        \
                           .zoffset = 0

gui_btn_state gui_btn(
	gui_btn_state   prior_state,
	const char     *text,
	struct btn_opts opts
);

int gui_check(
	int               prior_state,
	struct check_opts opts
);

void gui_edit(
	char            *text,
	size_t           maxlen,
	struct edit_opts opts
);

void gui_map(
	GLuint          texture,
	struct map_opts opts
);

void gui_rect(
	struct rect_opts opts
);

void gui_text(
	const char      *text,
	struct text_opts opts
);

void gui_text_center(
	const char      *text,
	float            width,
	struct text_opts opts
);

struct stack_opts {
	float hpadding;
	float vpadding;
	float xoffset;
	float yoffset;
	float zoffset;
};

struct window_opts {
	float hpadding;
	float vpadding;
	float xoffset;
	float yoffset;
	float zoffset;
};

#define STACK_OPTS_DEFAULTS .hpadding = 0, \
                            .vpadding = 0, \
                            .xoffset = 0, \
                            .yoffset = 0, \
                            .zoffset = 0
#define WINDOW_OPTS_DEFAULT STACK_OPTS_DEFAULTS

void gui_window_init(gui_container *, struct window_opts);
void gui_stack_init(gui_container *, struct stack_opts);
void gui_stack_break(gui_container *);
void gui_container_handle(gui_container *);
void gui_container_push(gui_container *);
void gui_container_pop(void);
void gui_current_container_add_element(float width, float height);
void gui_current_container_get_offsets(float[3]);
gui_container gui_current_container_get_state(void);
void gui_current_container_set_state(gui_container);

void gui_line(float x0, float y0, float z0, float t0, uint32_t c0,
              float x1, float y1, float z1, float t1, uint32_t c1);

float gui_char_width(float font_size);

#endif /* HAMMER_GUI_H_ */
