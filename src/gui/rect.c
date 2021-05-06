#include "hammer/gui/rect.h"
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
	glBufferStorage(GL_ARRAY_BUFFER, RECT_VBO_SIZE, 0, flags);
	frame->vb = glMapBufferRange(GL_ARRAY_BUFFER, 0, RECT_VBO_SIZE, flags);
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
			   1, GL_FALSE, (float *)window.ortho_matrix);
	glBindBuffer(GL_ARRAY_BUFFER, frame->vbo);
	glDrawArrays(GL_TRIANGLES, 0, frame->vb_vc);
	frame->vb_vc = 0;
}

void
gui_rect(gui_container *container, struct rect_opts opts)
{
	float container_offset[3] = { 0, 0, 0 };
	if (container) {
		float w = opts.xoffset + opts.width;
		float h = opts.yoffset + opts.height;
		gui_container_get_offsets(container, container_offset);
		gui_container_add_element(container, w, h);
	}
	opts.xoffset += container_offset[0];
	opts.yoffset += container_offset[1];
	opts.zoffset += container_offset[2];

	struct gui_rect_frame *frame = &window.current_frame->gui_rect_frame;

	const size_t VS = sizeof(struct gui_rect_vert);
	float r = ((opts.color & 0xff000000) >> 24) / 255.0f;
	float g = ((opts.color & 0x00ff0000) >> 16) / 255.0f;
	float b = ((opts.color & 0x0000ff00) >>  8) / 255.0f;
	float a = ((opts.color & 0x000000ff) >>  0) / 255.0f;
	float x0 = opts.xoffset;
	float x1 = opts.xoffset + opts.width;
	float y0 = opts.yoffset;
	float y1 = opts.yoffset + opts.height;
	float z = opts.zoffset;

	if ((frame->vb_vc + 6) * VS >= RECT_VBO_SIZE)
		return;

	struct gui_rect_vert vs[6] = {
		{ .position = {x0, y0, z}, .color = {r, g, b, a} },
		{ .position = {x1, y1, z}, .color = {r, g, b, a} },
		{ .position = {x1, y0, z}, .color = {r, g, b, a} },

		{ .position = {x0, y1, z}, .color = {r, g, b, a} },
		{ .position = {x1, y1, z}, .color = {r, g, b, a} },
		{ .position = {x0, y0, z}, .color = {r, g, b, a} }
	};
	size_t vi = frame->vb_vc;
	memcpy(frame->vb + vi, vs, VS * 6);
	frame->vb_vc += 6;
}
