#include "hammer/gui/img.h"
#include "hammer/gui.h"
#include "hammer/error.h"
#include "hammer/glsl.h"
#include "hammer/window.h"

void
gui_img_renderer_create(struct gui_img_renderer *renderer)
{
	renderer->shader = compile_shader_program(
	                     "resources/shaders/gui_img.vs",
	                     NULL, /* no geometry shader */
	                     "resources/shaders/gui_img.fs");
	if (renderer->shader == 0)
		xpanic("Error creating GUI img shader");
	glUseProgram(renderer->shader);

	renderer->uniforms.img = glGetUniformLocation(renderer->shader, "img");
	renderer->uniforms.ortho = glGetUniformLocation(renderer->shader, "ortho");
	renderer->uniforms.position = glGetUniformLocation(renderer->shader, "position");
	renderer->uniforms.dimensions = glGetUniformLocation(renderer->shader, "dimensions");

	glGenVertexArrays(1, &renderer->vao);
	glBindVertexArray(renderer->vao);

	glEnableVertexAttribArray(0);

	glGenBuffers(1, &renderer->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
	size_t VS = sizeof(GLfloat) * 2;
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, VS, NULL);

	GLfloat vs[6][2] = {
		{0, 0}, {1, 1}, {1, 0},
		{0, 1}, {1, 1}, {0, 0}
	};
	glBufferData(GL_ARRAY_BUFFER, VS*6, vs, GL_STATIC_DRAW);

	/* Constant uniform values */
	glUniform1i(renderer->uniforms.img, 0);
}

void
gui_img_renderer_destroy(struct gui_img_renderer *renderer)
{
	glDeleteBuffers(1, &renderer->vbo);
	glDeleteVertexArrays(1, &renderer->vao);
	glDeleteProgram(renderer->shader);
}

void
gui_img(GLuint texture, struct img_opts opts)
{
	struct gui_img_renderer *renderer = &window.gui_img_renderer;
	
	glUseProgram(renderer->shader);
	glBindVertexArray(renderer->vao);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	glUniform3f(renderer->uniforms.position,
	            opts.xoffset,
	            opts.yoffset,
	            opts.zoffset);
	glUniform2f(renderer->uniforms.dimensions,
	            opts.width,
	            opts.height);
	glUniformMatrix4fv(renderer->uniforms.ortho,
			   1, GL_FALSE, (float *)window.ortho_matrix);
	glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}
