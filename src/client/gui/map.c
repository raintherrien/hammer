#include "hammer/client/glsl.h"
#include "hammer/client/gui.h"
#include "hammer/client/gui/map.h"
#include "hammer/client/window.h"
#include "hammer/error.h"

void
gui_map_renderer_create(struct gui_map_renderer *renderer)
{
	renderer->shader = compile_shader_program(
	                     "resources/shaders/gui_map.vs",
	                     NULL, /* no geometry shader */
	                     "resources/shaders/gui_map.fs");
	if (renderer->shader == 0)
		xpanic("Error creating GUI map shader");
	glUseProgram(renderer->shader);

	renderer->uniforms.map = glGetUniformLocation(renderer->shader, "map");
	renderer->uniforms.ortho = glGetUniformLocation(renderer->shader, "ortho");
	renderer->uniforms.position = glGetUniformLocation(renderer->shader, "position");
	renderer->uniforms.dimensions = glGetUniformLocation(renderer->shader, "dimensions");
	renderer->uniforms.translation = glGetUniformLocation(renderer->shader, "translation");
	renderer->uniforms.scale = glGetUniformLocation(renderer->shader, "scale");

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
	glUniform1i(renderer->uniforms.map, 0);
}

void
gui_map_renderer_destroy(struct gui_map_renderer *renderer)
{
	glDeleteBuffers(1, &renderer->vbo);
	glDeleteVertexArrays(1, &renderer->vao);
	glDeleteProgram(renderer->shader);
}

void
gui_map(GLuint texture, struct map_opts opts)
{
	float container_offset[3];
	float w = opts.xoffset + opts.width;
	float h = opts.yoffset + opts.height;
	gui_current_container_get_offsets(container_offset);
	gui_current_container_add_element(w, h);
	opts.xoffset += container_offset[0];
	opts.yoffset += container_offset[1];
	opts.zoffset += container_offset[2];

	struct gui_map_renderer *renderer = &window.gui_map_renderer;

	glUseProgram(renderer->shader);
	glBindVertexArray(renderer->vao);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	glUniform3f(renderer->uniforms.position, opts.xoffset, opts.yoffset, opts.zoffset);
	glUniform2f(renderer->uniforms.dimensions, opts.width, opts.height);
	glUniform2f(renderer->uniforms.translation, opts.tran_x, opts.tran_y);
	glUniform2f(renderer->uniforms.scale, opts.scale_x, opts.scale_y);
	glUniformMatrix4fv(renderer->uniforms.ortho, 1, GL_FALSE, (float *)window.ortho_matrix);
	glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}
