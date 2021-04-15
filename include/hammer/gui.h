#ifndef HAMMER_GUI_H_
#define HAMMER_GUI_H_

#define gui_text_ratio 1

void gui_create(void);
void gui_destroy(void);
void gui_render(void);

void gui_text(const char *, float left, float top, float width, float height);

#endif /* HAMMER_GUI_H_ */