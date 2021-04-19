#include "hammer/gui.h"
#include "hammer/error.h"
#include "hammer/glsl.h"
#include "hammer/image.h"
#include "hammer/window.h"
#include <assert.h>
#include <cglm/mat4.h>
#include <cglm/cam.h>
#include <SDL2/SDL_image.h>

#define RECT_VBO_SIZE 1048576

struct rect_vert {
	GLfloat position[3];
	GLfloat color[4];
};

/*
 * Global state of rect renderer.
 */
struct {
	struct rect_vert *buffer[FRAMES_IN_FLIGHT];
	size_t buffer_count[FRAMES_IN_FLIGHT];
	GLuint shader;
	GLuint uniform_ortho;
	GLuint vao;
	GLuint vbo[FRAMES_IN_FLIGHT];
} gui_rect_renderer;

void
gui_rect_init(void)
{
	gui_rect_renderer.shader = compile_shader_program(
	                             "resources/shaders/gui_rect.vs",
	                             NULL, /* no GS */
	                             "resources/shaders/gui_rect.fs");
	if (gui_rect_renderer.shader == 0)
		xpanic("Error creating GUI rect shader");
	glUseProgram(gui_rect_renderer.shader);

	gui_rect_renderer.uniform_ortho = glGetUniformLocation(gui_rect_renderer.shader, "ortho");

	glGenVertexArrays(1, &gui_rect_renderer.vao);
	glBindVertexArray(gui_rect_renderer.vao);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	glGenBuffers(FRAMES_IN_FLIGHT, gui_rect_renderer.vbo);
	GLbitfield flags = GL_MAP_WRITE_BIT |
	                   GL_MAP_PERSISTENT_BIT |
	                   GL_MAP_COHERENT_BIT;
	for (size_t i = 0; i < FRAMES_IN_FLIGHT; ++ i) {
		glBindBuffer(GL_ARRAY_BUFFER, gui_rect_renderer.vbo[i]);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct rect_vert), (void *)offsetof(struct rect_vert,position));
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(struct rect_vert), (void *)offsetof(struct rect_vert,color));
		glBufferStorage(GL_ARRAY_BUFFER, RECT_VBO_SIZE, 0, flags);
		gui_rect_renderer.buffer[i] = glMapBufferRange(GL_ARRAY_BUFFER, 0,
		                                               RECT_VBO_SIZE, flags);
		gui_rect_renderer.buffer_count[i] = 0;
	}
}

void
gui_rect_deinit(void)
{
	glBindVertexArray(gui_rect_renderer.vao);
	for (size_t i = 0; i < FRAMES_IN_FLIGHT; ++ i)
		glUnmapNamedBuffer(gui_rect_renderer.vbo[i]);

	glDeleteBuffers(FRAMES_IN_FLIGHT, gui_rect_renderer.vbo);
	glDeleteVertexArrays(1, &gui_rect_renderer.vao);
	glDeleteProgram(gui_rect_renderer.shader);
}

void
gui_rect_render(void)
{
	size_t f = window.current_frame % FRAMES_IN_FLIGHT;

	/* 0,0 top left */
	mat4 ortho_matrix;
	glm_ortho(0, window.width, window.height, 0, -1000, 1, ortho_matrix);

	if (gui_rect_renderer.buffer_count[f]) {
		glUseProgram(gui_rect_renderer.shader);
		glBindVertexArray(gui_rect_renderer.vao);
		glUniformMatrix4fv(gui_rect_renderer.uniform_ortho,
		                   1, GL_FALSE, (float *)ortho_matrix);
		glBindBuffer(GL_ARRAY_BUFFER, gui_rect_renderer.vbo[f]);
		glDrawArrays(GL_TRIANGLES, 0, gui_rect_renderer.buffer_count[f]);
		gui_rect_renderer.buffer_count[f] = 0;
	}
}

void
gui_rect(struct rect_opts opts)
{
	const size_t VS = sizeof(struct rect_vert);
	size_t f = (window.current_frame + 1) % FRAMES_IN_FLIGHT;
	float r = ((opts.color & 0xff000000) >> 24) / 255.0f;
	float g = ((opts.color & 0x00ff0000) >> 16) / 255.0f;
	float b = ((opts.color & 0x0000ff00) >>  8) / 255.0f;
	float a = ((opts.color & 0x000000ff) >>  0) / 255.0f;
	float x0 = opts.xoffset;
	float x1 = opts.xoffset + opts.width;
	float y0 = opts.yoffset;
	float y1 = opts.yoffset + opts.height;

	if ((gui_rect_renderer.buffer_count[f]+6) * VS >= RECT_VBO_SIZE)
		return;

	struct rect_vert vs[6] = {
		{ .position = {x0, y0, 0}, .color = {r, g, b, a} },
		{ .position = {x1, y1, 0}, .color = {r, g, b, a} },
		{ .position = {x1, y0, 0}, .color = {r, g, b, a} },

		{ .position = {x0, y1, 0}, .color = {r, g, b, a} },
		{ .position = {x1, y1, 0}, .color = {r, g, b, a} },
		{ .position = {x0, y0, 0}, .color = {r, g, b, a} }
	};
	size_t vi = gui_rect_renderer.buffer_count[f];
	memcpy(gui_rect_renderer.buffer[f] + vi, vs, VS * 6);
	gui_rect_renderer.buffer_count[f] += 6;
}
