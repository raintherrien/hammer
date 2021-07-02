#ifndef HAMMER_CLIENT_GUI_TEXT_H_
#define HAMMER_CLIENT_GUI_TEXT_H_

#include "hammer/client/gui.h"

struct gui_text_vert {
	GLfloat position[3];
	GLfloat dimensions[2];
	GLuint  color;
	GLubyte weight;
	GLubyte style;
	GLubyte character;
};

struct gui_text_font {
	GLenum texture_format;
	GLuint texture_array;
	int    texture_width;
	int    texture_height;
	float  character_ratio;
};

struct gui_text_frame {
	struct gui_text_vert *vb;
	size_t vb_vc;
	GLuint vbo;
};

struct gui_text_renderer {
	struct gui_text_font font_regular;
	GLuint shader;
	GLuint vao;
	struct {
		GLuint font_sampler;
		GLuint font_height;
		GLuint ortho;
	} uniforms;
};

void gui_text_renderer_create(struct gui_text_renderer *);
void gui_text_renderer_destroy(struct gui_text_renderer *);
void gui_text_frame_create(struct gui_text_renderer *, struct gui_text_frame *);
void gui_text_frame_destroy(struct gui_text_frame *);
void gui_text_render(struct gui_text_renderer *, struct gui_text_frame *);

#endif /* HAMMER_CLIENT_GUI_TEXT_H_ */
