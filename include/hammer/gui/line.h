#ifndef HAMMER_GUI_LINE_H_
#define HAMMER_GUI_LINE_H_

#include "hammer/gui.h"

struct gui_line_vert {
	GLfloat position[3];
	GLfloat thickness;
	GLuint  color;
};

struct gui_line_frame {
	struct gui_line_vert *vb;
	size_t vb_vc;
	GLuint vbo;
};

struct gui_line_renderer {
	GLuint shader;
	GLuint vao;
	GLuint uniform_ortho;
};

void gui_line_renderer_create(struct gui_line_renderer *);
void gui_line_renderer_destroy(struct gui_line_renderer *);
void gui_line_frame_create(struct gui_line_renderer *, struct gui_line_frame *);
void gui_line_frame_destroy(struct gui_line_frame *);
void gui_line_render(struct gui_line_renderer *, struct gui_line_frame *);

#endif /* HAMMER_GUI_LINE_H_ */
