#ifndef HAMMER_GUI_H_
#define HAMMER_GUI_H_

void gui_create(void);
void gui_destroy(void);
void gui_render(void);

void gui_text(const char *, float left, float top, float font_size, float r, float g, float b);

#endif /* HAMMER_GUI_H_ */
