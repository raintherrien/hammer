#ifndef HAMMER_GUI_IMG_H_
#define HAMMER_GUI_IMG_H_

#include "hammer/gui.h"

struct gui_img_renderer {
	GLuint shader;
	GLuint vao;
	GLuint vbo;
	struct {
		GLuint img;
		GLuint ortho;
		GLuint position;
		GLuint dimensions;
	} uniforms;
};

void gui_img_renderer_create(struct gui_img_renderer *);
void gui_img_renderer_destroy(struct gui_img_renderer *);

#endif /* HAMMER_GUI_IMG_H_ */
