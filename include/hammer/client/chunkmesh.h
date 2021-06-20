#ifndef HAMMER_CLIENT_CHUNKMESH_H_
#define HAMMER_CLIENT_CHUNKMESH_H_

#include <GL/glew.h>

struct chunk;

struct chunkmesh {
	GLsizei vc;
	GLuint  vao;
	GLuint  vbo;
};

void chunkmesh_gl_create(struct chunkmesh *, const struct chunk *);
void chunkmesh_gl_destroy(struct chunkmesh *);

struct chunkmesh_renderer {
	GLuint shader;
	struct {
		GLuint mvp;
	} uniforms;
};

extern struct chunkmesh_renderer chunkmesh_renderer;

void chunkmesh_renderer_gl_create(void);
void chunkmesh_renderer_gl_destroy(void);

#endif /* HAMMER_CLIENT_CHUNKMESH_H_ */
