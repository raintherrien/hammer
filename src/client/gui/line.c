#include "hammer/client/gui/line.h"
#include "hammer/client/glsl.h"
#include "hammer/client/window.h"
#include "hammer/error.h"
#include "hammer/image.h"
#include <assert.h>
#include <SDL2/SDL_image.h>

#define LINE_VBO_SIZE 1048576

void
gui_line_renderer_create(struct gui_line_renderer *renderer)
{
	renderer->shader = compile_shader_program(
	                     "resources/shaders/gui_line.vs",
	                     "resources/shaders/gui_line.gs",
	                     "resources/shaders/gui_line.fs");
	if (renderer->shader == 0)
		xpanic("Error creating GUI line shader");
	glUseProgram(renderer->shader);

	renderer->uniform_ortho = glGetUniformLocation(renderer->shader, "ortho");

	glGenVertexArrays(1, &renderer->vao);
	glBindVertexArray(renderer->vao);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
}

void
gui_line_renderer_destroy(struct gui_line_renderer *renderer)
{
	glDeleteVertexArrays(1, &renderer->vao);
	glDeleteProgram(renderer->shader);
}

void
gui_line_frame_create(struct gui_line_renderer *renderer,
                      struct gui_line_frame    *frame)
{
	glBindVertexArray(renderer->vao);

	glGenBuffers(1, &frame->vbo);
	GLbitfield flags = GL_MAP_WRITE_BIT |
	                   GL_MAP_PERSISTENT_BIT |
	                   GL_MAP_COHERENT_BIT;
	glBindBuffer(GL_ARRAY_BUFFER, frame->vbo);
	const size_t VS = sizeof(struct gui_line_vert);
	const size_t offset[6] = {
		offsetof(struct gui_line_vert, position),
		offsetof(struct gui_line_vert, thickness),
		offsetof(struct gui_line_vert, color)
	};
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, VS, (void *)offset[0]);
	glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, VS, (void *)offset[1]);
	glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT,  VS, (void *)offset[2]);
	glBufferStorage(GL_ARRAY_BUFFER, LINE_VBO_SIZE, 0, flags);
	frame->vb = glMapBufferRange(GL_ARRAY_BUFFER, 0, LINE_VBO_SIZE, flags);
	frame->vb_vc = 0;
}

void
gui_line_frame_destroy(struct gui_line_frame    *frame)
{
	glUnmapNamedBuffer(frame->vbo);
	glDeleteBuffers(1, &frame->vbo);
}

void
gui_line_render(struct gui_line_renderer *renderer,
                struct gui_line_frame    *frame)
{
	if (frame->vb_vc == 0)
		return;

	glUseProgram(renderer->shader);
	glBindVertexArray(renderer->vao);
	glUniformMatrix4fv(renderer->uniform_ortho, 1, GL_FALSE,
	                   (float *)window.ortho_matrix);
	glBindBuffer(GL_ARRAY_BUFFER, frame->vbo);
	glDrawArrays(GL_LINES, 0, frame->vb_vc);
	frame->vb_vc = 0;
}

void
gui_line(float x0, float y0, float z0, float t0, uint32_t c0,
         float x1, float y1, float z1, float t1, uint32_t c1)
{
	struct gui_line_frame *frame = &window.current_frame->gui_line_frame;

	if (LINE_VBO_SIZE <= frame->vb_vc * sizeof(struct gui_line_vert))
		return;

	frame->vb[frame->vb_vc ++] = (struct gui_line_vert) {
		.position = {x0, y0, z0},
		.thickness = t0,
		.color  = c0
	};
	frame->vb[frame->vb_vc ++] = (struct gui_line_vert) {
		.position = {x1, y1, z1},
		.thickness = t1,
		.color  = c1
	};
}
