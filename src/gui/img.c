#include "hammer/gui.h"
#include "hammer/error.h"
#include "hammer/glsl.h"
#include "hammer/window.h"
#include <cglm/mat4.h>
#include <cglm/cam.h>

/*
 * Global state of img renderer.
 */
struct {
	GLuint shader;
	GLuint vao;
	GLuint vbo;
	struct gui_img_renderer_uniforms {
		GLuint img;
		GLuint ortho;
		GLuint position;
		GLuint dimensions;
	} uniforms;
} gui_img_renderer;

void
gui_img_init(void)
{
	gui_img_renderer.shader = compile_shader_program(
	                            "resources/shaders/gui_img.vs",
	                            NULL, /* no geometry shader */
	                            "resources/shaders/gui_img.fs");
	if (gui_img_renderer.shader == 0)
		xpanic("Error creating GUI img shader");
	glUseProgram(gui_img_renderer.shader);

	gui_img_renderer.uniforms = (struct gui_img_renderer_uniforms) {
		.img = glGetUniformLocation(gui_img_renderer.shader, "img"),
		.ortho = glGetUniformLocation(gui_img_renderer.shader, "ortho"),
		.position = glGetUniformLocation(gui_img_renderer.shader, "position"),
		.dimensions = glGetUniformLocation(gui_img_renderer.shader, "dimensions")
	};

	glGenVertexArrays(1, &gui_img_renderer.vao);
	glBindVertexArray(gui_img_renderer.vao);

	glEnableVertexAttribArray(0);

	glGenBuffers(1, &gui_img_renderer.vbo);
	glBindBuffer(GL_ARRAY_BUFFER, gui_img_renderer.vbo);
	size_t VS = sizeof(GLfloat) * 2;
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, VS, NULL);

	GLfloat vs[6][2] = {
		{0, 0}, {1, 1}, {1, 0},
		{0, 1}, {1, 1}, {0, 0}
	};
	glBufferData(GL_ARRAY_BUFFER, VS*6, vs, GL_STATIC_DRAW);

	/* Constant uniform values */
	glUniform1i(gui_img_renderer.uniforms.img, 0);
}

void
gui_img_deinit(void)
{
	glBindVertexArray(gui_img_renderer.vao);
	glDeleteBuffers(1, &gui_img_renderer.vbo);
	glDeleteVertexArrays(1, &gui_img_renderer.vao);
	glDeleteProgram(gui_img_renderer.shader);
}

void
gui_img(GLuint texture, struct img_opts opts)
{
	/* 0,0 top left */
	mat4 ortho_matrix;
	glm_ortho(0, window.width, window.height, 0, -1000, 1, ortho_matrix);

	glUseProgram(gui_img_renderer.shader);
	glBindVertexArray(gui_img_renderer.vao);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	glUniform3f(gui_img_renderer.uniforms.position,
	            opts.xoffset,
	            opts.yoffset,
	            opts.zoffset);
	glUniform2f(gui_img_renderer.uniforms.dimensions,
	            opts.width,
	            opts.height);
	glUniformMatrix4fv(gui_img_renderer.uniforms.ortho,
			   1, GL_FALSE, (float *)ortho_matrix);
	glBindBuffer(GL_ARRAY_BUFFER, gui_img_renderer.vbo);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}
