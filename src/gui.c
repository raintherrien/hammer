#include "hammer/gui.h"
#include "hammer/error.h"
#include "hammer/glsl.h"
#include "hammer/image.h"
#include "hammer/window.h"
#include <cglm/mat4.h>
#include <cglm/cam.h>

#define TEXT_VBO_SIZE 8192*10

float gui_fr;

struct {
	void  *text_buffer[FRAMES_IN_FLIGHT];
	size_t text_buffer_count[FRAMES_IN_FLIGHT];

	GLuint text_shader;
	GLuint text_vao;
	GLuint text_vbo[FRAMES_IN_FLIGHT];

	GLuint font_atlas;
	int font_atlas_w;
	int font_atlas_h;

	mat4 ortho_matrix;
} gui;

struct font_vertex {
	float xyz[3];
	float uv[2];
	float s;
};

void
gui_create(void)
{
	gui.text_shader = compile_shader_program("resources/shaders/gui_text.vs",
	                                         "resources/shaders/gui_text.fs");
	if (gui.text_shader == 0)
		xpanic("Error creating GUI text shader");

	gui.font_atlas = load_texture("resources/fonts/jetbrains_mono_regular_sdf.png",
	                              &gui.font_atlas_w,
	                              &gui.font_atlas_h);
	if (gui.font_atlas == 0)
		xpanic("Error loading GUI font atlas");
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	gui_fr = (gui.font_atlas_w / 95.0f) / gui.font_atlas_h;

	glGenVertexArrays(1, &gui.text_vao);
	glBindVertexArray(gui.text_vao);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	glGenBuffers(FRAMES_IN_FLIGHT, gui.text_vbo);
	GLbitfield flags = GL_MAP_WRITE_BIT |
	                   GL_MAP_PERSISTENT_BIT |
	                   GL_MAP_COHERENT_BIT;
	for (size_t i = 0; i < FRAMES_IN_FLIGHT; ++ i) {
		glBindBuffer(GL_ARRAY_BUFFER, gui.text_vbo[i]);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct font_vertex), (void *)offsetof(struct font_vertex,xyz));
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(struct font_vertex), (void *)offsetof(struct font_vertex,uv));
		glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(struct font_vertex), (void *)offsetof(struct font_vertex,s));
		glBufferStorage(GL_ARRAY_BUFFER, TEXT_VBO_SIZE, 0, flags);
		gui.text_buffer[i] = glMapBufferRange(GL_ARRAY_BUFFER, 0,
		                                      TEXT_VBO_SIZE, flags);
	}

	/* XXX Never resized */
	/* 0,0 top left */
	glm_ortho(0, window.width, window.height, 0, 0, 1, gui.ortho_matrix);
}

void
gui_destroy(void)
{
	glBindVertexArray(gui.text_vao);
	glUnmapBuffer(GL_ARRAY_BUFFER);

	glDeleteTextures(1, &gui.font_atlas);
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
		glUniformMatrix4fv(glGetUniformLocation(gui.text_shader, "ortho"),
		                   1, GL_FALSE, (float *)gui.ortho_matrix);
		glBindBuffer(GL_ARRAY_BUFFER, gui.text_vbo[f]);
		glDrawArrays(GL_TRIANGLES, 0, gui.text_buffer_count[f]);
		gui.text_buffer_count[f] = 0;
	}
}

void
gui_text(const char *text, float left, float top, float font_size)
{
	size_t f = (window.current_frame + 1) % FRAMES_IN_FLIGHT;
	/* XXX No overflow check */
	float fw = 1.0f / 95;
	size_t l = strlen(text);
	float ww = font_size * gui_fr;
	float s = font_size / gui.font_atlas_h;
	for (size_t i = 0; i < l; ++ i) {
		int c = text[i] - ' ';
		float u0 = c * fw;
		float u1 = (c + 1) * fw;
		float x0 = left + ww * i;
		float x1 = left + ww * (i + 1);
		float y0 = top;
		float y1 = top + font_size;
		struct font_vertex v[6] = {
			{{x0,y0,0}, {u0,0}, s },
			{{x1,y1,0}, {u1,1}, s },
			{{x1,y0,0}, {u1,0}, s },

			{{x1,y1,0}, {u1,1}, s },
			{{x0,y0,0}, {u0,0}, s },
			{{x0,y1,0}, {u0,1}, s },
		};
		const size_t VS = sizeof(struct font_vertex);
		memcpy((char *)gui.text_buffer[f] + gui.text_buffer_count[f] * VS, v, VS*6);
		gui.text_buffer_count[f] += 6;
	}
}
