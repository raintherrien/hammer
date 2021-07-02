#ifndef HAMMER_CLIENT_GUI_RECT_H_
#define HAMMER_CLIENT_GUI_RECT_H_

#include "hammer/client/gui.h"

struct gui_rect_vert {
	GLfloat position[3];
	GLfloat color[4];
};

struct gui_rect_frame {
	struct gui_rect_vert *vb;
	size_t vb_vc;
	GLuint vbo;
};

struct gui_rect_renderer {
	GLuint shader;
	GLuint uniform_ortho;
	GLuint vao;
};

void gui_rect_renderer_create(struct gui_rect_renderer *);
void gui_rect_renderer_destroy(struct gui_rect_renderer *);
void gui_rect_frame_create(struct gui_rect_renderer *, struct gui_rect_frame *);
void gui_rect_frame_destroy(struct gui_rect_frame *);
void gui_rect_render(struct gui_rect_renderer *, struct gui_rect_frame *);

#endif /* HAMMER_CLIENT_GUI_RECT_H_ */
