#ifndef HAMMER_GUI_MAP_H_
#define HAMMER_GUI_MAP_H_

#include "hammer/gui.h"

struct gui_map_renderer {
	GLuint shader;
	GLuint vao;
	GLuint vbo;
	struct {
		GLuint map;
		GLuint ortho;
		GLuint position;
		GLuint dimensions;
		GLuint translation;
		GLuint scale;
	} uniforms;
};

void gui_map_renderer_create(struct gui_map_renderer *);
void gui_map_renderer_destroy(struct gui_map_renderer *);

#endif /* HAMMER_GUI_MAP_H_ */
