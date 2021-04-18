#ifndef HAMMER_GUI_H_
#define HAMMER_GUI_H_

#include <stdint.h>

void gui_create(void);
void gui_destroy(void);
void gui_render(void);

enum font_style {
	FONT_STYLE_ITALIC        = 1<<0,
	/* TODO: Other style elements
	FONT_STYLE_UNDERLINE     = 1<<2,
	FONT_STYLE_STRIKETHROUGH = 1<<3
	*/
};

struct font_opts {
	float           size;
	float           weight;
	float           z;
	enum font_style style;
	uint32_t        color;
};

#define FONT_OPTS_DEFAULTS .weight = 128,      \
                           .z = 0,          \
                           .style = 0,         \
                           .color = 0xffffffff

void gui_text_impl(const char *txt, float left, float top, struct font_opts);
void gui_text_center_impl(const char *txt, float top, struct font_opts);

#define gui_text(TXT,L,T,...) gui_text_impl(TXT,L,T, (struct font_opts) { FONT_OPTS_DEFAULTS, .size = __VA_ARGS__ })
#define gui_text_center(TXT,T,...) gui_text_center_impl(TXT,T, (struct font_opts) { FONT_OPTS_DEFAULTS, .size = __VA_ARGS__ })

#endif /* HAMMER_GUI_H_ */
