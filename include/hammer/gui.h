#ifndef HAMMER_GUI_H_
#define HAMMER_GUI_H_

extern float gui_font_ratio;

void gui_create(void);
void gui_destroy(void);
void gui_render(void);

void gui_text(const char *, float left, float top, float width, float height);

#endif /* HAMMER_GUI_H_ */
