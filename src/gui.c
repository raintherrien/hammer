#include "hammer/gui.h"
#include "hammer/error.h"
#include "hammer/glsl.h"
#include "hammer/window.h"

#define TEXT_VBO_SIZE 1024

struct {
	void  *text_buffer[FRAMES_IN_FLIGHT];
	size_t text_buffer_count[FRAMES_IN_FLIGHT];

	GLuint text_shader;
	GLuint text_vao;
	GLuint text_vbo[FRAMES_IN_FLIGHT];
} gui;

void
gui_create(void)
{
	gui.text_shader = compile_shader_program("resources/shaders/gui_text.vs",
	                                         "resources/shaders/gui_text.fs");
	if (gui.text_shader == 0)
		xpanic("Error creating GUI text shader");

	glGenVertexArrays(1, &gui.text_vao);
	glBindVertexArray(gui.text_vao);

	glEnableVertexAttribArray(0);

	glGenBuffers(FRAMES_IN_FLIGHT, gui.text_vbo);
	GLbitfield flags = GL_MAP_WRITE_BIT |
	                   GL_MAP_PERSISTENT_BIT |
	                   GL_MAP_COHERENT_BIT;
	for (size_t i = 0; i < FRAMES_IN_FLIGHT; ++ i) {
		glBindBuffer(GL_ARRAY_BUFFER, gui.text_vbo[i]);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*3, 0);
		glBufferStorage(GL_ARRAY_BUFFER, TEXT_VBO_SIZE, 0, flags);
		gui.text_buffer[i] = glMapBufferRange(GL_ARRAY_BUFFER, 0,
		                                      TEXT_VBO_SIZE, flags);
	}
}

void
gui_destroy(void)
{
	glBindVertexArray(gui.text_vao);
	glUnmapBuffer(GL_ARRAY_BUFFER);

	glDeleteBuffers(1, gui.text_vbo);
	glDeleteVertexArrays(1, &gui.text_vao);
	glDeleteProgram(gui.text_shader);
}

void
gui_render(void)
{
	size_t f = window.current_frame % FRAMES_IN_FLIGHT;

	if (gui.text_buffer_count[f]) {
		glUseProgram(gui.text_shader);
		glBindVertexArray(gui.text_vao);
		glBindBuffer(GL_ARRAY_BUFFER, gui.text_vbo[f]);
		glDrawArrays(GL_TRIANGLES, 0, gui.text_buffer_count[f]);
		gui.text_buffer_count[f] = 0;
	}
}

void
gui_text(const char *text, float left, float top, float width, float height)
{
	size_t f = (window.current_frame + 1) % FRAMES_IN_FLIGHT;
	/* XXX No overflow check */
	float v0[3] = { -1, -1, 0 }, v1[3] = { 1, -1, 0 }, v2[3] = { 0, 1, 0 };
	const size_t VS = sizeof(float) * 3;
	memcpy((char *)gui.text_buffer[f] + (gui.text_buffer_count[f]+0)*VS, v0, VS);
	memcpy((char *)gui.text_buffer[f] + (gui.text_buffer_count[f]+1)*VS, v1, VS);
	memcpy((char *)gui.text_buffer[f] + (gui.text_buffer_count[f]+2)*VS, v2, VS);
	gui.text_buffer_count[f] += 3;
}
