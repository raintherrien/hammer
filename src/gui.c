#include "hammer/gui.h"
#include "hammer/error.h"
#include "hammer/glsl.h"
#include "hammer/image.h"
#include "hammer/window.h"
#include <assert.h>
#include <cglm/mat4.h>
#include <cglm/cam.h>
#include <SDL2/SDL_image.h>

#define TEXT_VBO_SIZE 1048576
#define FONT_ATLAS_LAYERS ((int)'~' - '!')
_Static_assert(FONT_ATLAS_LAYERS == 93);

static struct font_atlas load_font_atlas(const char *filename);

struct font_atlas {
	GLenum texture_format;
	GLuint texture_array;
	int    texture_width;
	int    texture_height;
	float  character_ratio;
};

struct {
	struct text_vertex *text_buffer[FRAMES_IN_FLIGHT];
	size_t              text_buffer_count[FRAMES_IN_FLIGHT];

	GLuint text_shader;
	GLuint text_vao;
	GLuint text_vbo[FRAMES_IN_FLIGHT];

	GLuint uniform_font_atlas_height_location;
	GLuint uniform_ortho_location;

	struct font_atlas regular_atlas;
} gui;

struct text_vertex { // 23B
	GLfloat position[3];
	GLfloat dimensions[2];
	GLuint  color;
	GLubyte weight;
	GLubyte style;
	GLubyte character;
};

void
gui_create(void)
{
	gui.text_shader = compile_shader_program("resources/shaders/gui_text.vs",
	                                         "resources/shaders/gui_text.gs",
	                                         "resources/shaders/gui_text.fs");
	if (gui.text_shader == 0)
		xpanic("Error creating GUI text shader");
	gui.uniform_font_atlas_height_location = glGetUniformLocation(gui.text_shader, "font_atlas_height");
	gui.uniform_ortho_location = glGetUniformLocation(gui.text_shader, "ortho");

	gui.regular_atlas = load_font_atlas("JetBrainsMono-Regular");
	if (gui.regular_atlas.texture_array == 0)
		xpanic("Error loading GUI font atlas");

	glGenVertexArrays(1, &gui.text_vao);
	glBindVertexArray(gui.text_vao);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	glEnableVertexAttribArray(3);
	glEnableVertexAttribArray(4);
	glEnableVertexAttribArray(5);

	glGenBuffers(FRAMES_IN_FLIGHT, gui.text_vbo);
	GLbitfield flags = GL_MAP_WRITE_BIT |
	                   GL_MAP_PERSISTENT_BIT |
	                   GL_MAP_COHERENT_BIT;
	for (size_t i = 0; i < FRAMES_IN_FLIGHT; ++ i) {
		glBindBuffer(GL_ARRAY_BUFFER, gui.text_vbo[i]);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct text_vertex), (void *)offsetof(struct text_vertex,position));
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(struct text_vertex), (void *)offsetof(struct text_vertex,dimensions));
		glVertexAttribIPointer(5, 1, GL_UNSIGNED_INT, sizeof(struct text_vertex), (void *)offsetof(struct text_vertex,color));
		glVertexAttribIPointer(2, 1, GL_UNSIGNED_BYTE, sizeof(struct text_vertex), (void *)offsetof(struct text_vertex,weight));
		glVertexAttribIPointer(3, 1, GL_UNSIGNED_BYTE, sizeof(struct text_vertex), (void *)offsetof(struct text_vertex,style));
		glVertexAttribIPointer(4, 1, GL_UNSIGNED_BYTE, sizeof(struct text_vertex), (void *)offsetof(struct text_vertex,character));
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

	glDeleteTextures(1, &gui.regular_atlas.texture_array);
	glDeleteBuffers(1, gui.text_vbo);
	glDeleteVertexArrays(1, &gui.text_vao);
	glDeleteProgram(gui.text_shader);
}

void
gui_render(void)
{
	size_t f = window.current_frame % FRAMES_IN_FLIGHT;

	/* 0,0 top left */
	mat4 ortho_matrix;
	glm_ortho(0, window.width, window.height, 0, -1000, 1, ortho_matrix);

	if (gui.text_buffer_count[f]) {
		glUseProgram(gui.text_shader);
		glBindVertexArray(gui.text_vao);
		glUniform1f(gui.uniform_font_atlas_height_location,
		            gui.regular_atlas.texture_height);
		glUniformMatrix4fv(gui.uniform_ortho_location,
		                   1, GL_FALSE, (float *)ortho_matrix);
		glBindBuffer(GL_ARRAY_BUFFER, gui.text_vbo[f]);
		glDrawArrays(GL_POINTS, 0, gui.text_buffer_count[f]);
		gui.text_buffer_count[f] = 0;
	}
}

void
gui_text_impl(const char *txt, float left, float top, struct font_opts opts)
{
	const size_t VS = sizeof(struct text_vertex);
	size_t f = (window.current_frame + 1) % FRAMES_IN_FLIGHT;
	size_t l = strlen(txt);
	float char_width = opts.size * gui.regular_atlas.character_ratio;
	for (size_t i = 0; i < l; ++ i) {
		if (gui.text_buffer_count[f] * VS >= TEXT_VBO_SIZE)
			continue;
		int c = (int)txt[i] - '!';
		if (c < 0 || c >= FONT_ATLAS_LAYERS)
			continue;
		float x = left + char_width * i;
		float y = top;
		struct text_vertex v = {
			.position = {x, y, opts.z},
			.dimensions = {char_width, opts.size},
			.color  = opts.color,
			.weight = opts.weight,
			.style  = opts.style,
			.character = c
		};
		memcpy((char *)gui.text_buffer[f] + gui.text_buffer_count[f] * VS, &v, sizeof(v));
		++ gui.text_buffer_count[f];
	}
}

void
gui_text_center_impl(const char *txt, float top, struct font_opts opts)
{
	size_t l = strlen(txt);
	float ww = opts.size * gui.regular_atlas.character_ratio;
	float left = (window.width - l * ww) / 2;
	gui_text_impl(txt, left, top, opts);
}

static struct font_atlas
load_font_atlas(const char *fontname)
{
	struct font_atlas atlas;
	glGenTextures(1, &atlas.texture_array);
	glBindTexture(GL_TEXTURE_2D_ARRAY, atlas.texture_array);

	SDL_Surface *surface;
	/* Note: We *skip* a texture for ' ' */
	for (int i = 0; i < FONT_ATLAS_LAYERS; ++ i) {
		char filename[64];
		snprintf(filename, 64-1, "resources/fonts/%s/%d.png", fontname, i+'!');
		surface = IMG_Load(filename);
		if (!surface) {
			fprintf(stderr,
			        ANSI_ESC_BOLDRED
				"Error loading font atlas image file %s: %s\n"
				ANSI_ESC_BOLDRED,
				filename,
				SDL_GetError());
			return (struct font_atlas) {};
		}
		GLenum format;
		GLenum internalformat;
		switch (surface->format->BytesPerPixel) {
		case 4:
			format = (SDL_BYTEORDER == SDL_BIG_ENDIAN ? GL_BGRA : GL_RGBA);
			internalformat = GL_RGBA8;
			break;
		case 3:
			format = (SDL_BYTEORDER == SDL_BIG_ENDIAN ? GL_BGR : GL_RGB);
			internalformat = GL_RGB8;
			break;
		case 2:
			format = GL_RG;
			internalformat = GL_RG8;
			break;
		case 1:
			format = GL_RED;
			internalformat = GL_R8;
			break;
		}

		/* Determine format from first image */
		if (i == 0) {
			atlas.texture_format = internalformat;
			glTexStorage3D(GL_TEXTURE_2D_ARRAY,
			               1, /* mipmaps */
			               atlas.texture_format,
			               surface->w, surface->h,
			               FONT_ATLAS_LAYERS); /* layers */
			atlas.texture_width  = surface->w;
			atlas.texture_height = surface->h;
			atlas.character_ratio = atlas.texture_width /
			                        (float)atlas.texture_height;
		}
		assert(atlas.texture_format == internalformat);
		assert(atlas.texture_width  == surface->w);
		assert(atlas.texture_height == surface->h);

		glTexSubImage3D(GL_TEXTURE_2D_ARRAY,
		                0,    /* mipmap level */
		                0, 0, i, /* x,y,z offsets */
		                surface->w, surface->h, 1, /* w,h,layer */
		                format,
		                GL_UNSIGNED_BYTE,
		                surface->pixels);

		SDL_FreeSurface(surface);
	}

	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	return atlas;
}
