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
	enum  { GUI_STACK } type;
	union { gui_stack stack; };
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
	gui_container  *container, /* optional */
	gui_btn_state   prior_state,
	const char     *text,
	struct btn_opts opts
);

int gui_check(
	gui_container    *container, /* optional */
	int               prior_state,
	struct check_opts opts
);

void gui_edit(
	gui_container   *container, /* optional */
	char            *text,
	size_t           maxlen,
	struct edit_opts opts
);

void gui_map(
	gui_container  *container, /* optional */
	GLuint          texture,
	struct map_opts opts
);

void gui_rect(
	gui_container   *container, /* optional */
	struct rect_opts opts
);

void gui_text(
	gui_container   *container, /* optional */
	const char      *text,
	struct text_opts opts
);

void gui_text_center(
	gui_container   *container, /* optional */
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

#define STACK_OPTS_DEFAULTS .hpadding = 0, \
                            .vpadding = 0, \
                            .xoffset = 0, \
                            .yoffset = 0, \
                            .zoffset = 0

void gui_stack_init(gui_container *, struct stack_opts);
void gui_stack_break(gui_container *);
void gui_container_add_element(gui_container *, float width, float height);
void gui_container_get_offsets(gui_container *, float[3]);

float gui_char_width(float font_size);

#endif /* HAMMER_GUI_H_ */
