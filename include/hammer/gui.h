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

#define EDIT_OPTS_DEFAULTS BTN_OPTS_DEFAULTS

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

gui_btn_state gui_btn(gui_btn_state prior_state,
                      const char *text, size_t len,
                      struct btn_opts);
void gui_edit(char *text, size_t maxlen, struct edit_opts);
void gui_rect(struct rect_opts);
void gui_text(const char *text, size_t len, struct text_opts);
void gui_text_center(const char *text, size_t len, float width, struct text_opts);

float gui_char_width(float font_size);

struct img_opts {
	float xoffset;
	float yoffset;
	float zoffset;
	float width;
	float height;
};

#define IMG_OPTS_DEFAULTS .xoffset = 0, \
                          .yoffset = 0, \
                          .zoffset = 0, \
                          .width   = 0, \
                          .height  = 0

void gui_img(GLuint texture, struct img_opts);

#endif /* HAMMER_GUI_H_ */
