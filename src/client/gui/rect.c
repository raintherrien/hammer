#include "hammer/client/glsl.h"
#include "hammer/client/gui.h"
#include "hammer/client/gui/rect.h"
#include "hammer/client/window.h"
#include "hammer/error.h"
#include "hammer/image.h"
#include <assert.h>
#include <SDL2/SDL_image.h>

void
gui_rect_renderer_create(struct gui_rect_renderer *renderer)
{
	renderer->shader = compile_shader_program(
	                     "resources/shaders/gui_rect.vs",
	                     NULL, /* no GS */
	                     "resources/shaders/gui_rect.fs");
	if (renderer->shader == 0)
		xpanic("Error creating GUI rect shader");
	glUseProgram(renderer->shader);

	renderer->uniform_ortho = glGetUniformLocation(renderer->shader, "ortho");

	glGenVertexArrays(1, &renderer->vao);
	glBindVertexArray(renderer->vao);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
}

void
gui_rect_renderer_destroy(struct gui_rect_renderer *renderer)
{
	glDeleteVertexArrays(1, &renderer->vao);
	glDeleteProgram(renderer->shader);
}

void
gui_rect_frame_create(struct gui_rect_renderer *renderer,
                      struct gui_rect_frame *frame)
{
	glBindVertexArray(renderer->vao);

	glGenBuffers(1, &frame->vbo);
	GLbitfield flags = GL_MAP_WRITE_BIT |
	                   GL_MAP_PERSISTENT_BIT |
	                   GL_MAP_COHERENT_BIT;
	const size_t VS = sizeof(struct gui_rect_vert);
	const size_t offset[2] = {
		offsetof(struct gui_rect_vert, position),
		offsetof(struct gui_rect_vert, color)
	};
	glBindBuffer(GL_ARRAY_BUFFER, frame->vbo);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, VS, (void *)offset[0]);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, VS, (void *)offset[1]);
	glBufferStorage(GL_ARRAY_BUFFER, GUI_RECT_VBO_SIZE, 0, flags);
	frame->vb = glMapBufferRange(GL_ARRAY_BUFFER, 0, GUI_RECT_VBO_SIZE, flags);
	frame->vb_vc = 0;
}

void
gui_rect_frame_destroy(struct gui_rect_frame *frame)
{
	glUnmapNamedBuffer(frame->vbo);
	glDeleteBuffers(1, &frame->vbo);
}

void
gui_rect_render(struct gui_rect_renderer *renderer,
                struct gui_rect_frame *frame)
{
	if (frame->vb_vc == 0)
		return;

	glUseProgram(renderer->shader);
	glBindVertexArray(renderer->vao);
	glUniformMatrix4fv(renderer->uniform_ortho,
			   1, GL_FALSE, (float *)window_ortho().raw);
	glBindBuffer(GL_ARRAY_BUFFER, frame->vbo);
	glDrawArrays(GL_TRIANGLES, 0, frame->vb_vc);
	frame->vb_vc = 0;
}
