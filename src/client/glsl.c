#include "hammer/client/glsl.h"
#include "hammer/error.h"
#include "hammer/file.h"
#include <stdio.h>
#include <stdlib.h>

GLuint
compile_shader_file(const char *filename, GLenum shader_type)
{
	char *src = read_file(filename);
	if (!src) {
		xperrorva("Error reading shader file %s", filename);
		return 0;
	}

	GLuint shader = glCreateShader(shader_type);
	glShaderSource(shader, 1, (const GLchar **)&src, NULL);
	glCompileShader(shader);

	free(src);

	GLint result;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
	if (result == GL_FALSE) {
		char log[1024];
		glGetShaderInfoLog(shader, 1023, NULL, log);
		errno = EINVAL;
		xperrorva("Error compiling shader file %s", filename);
		fprintf(stderr, "%s\n", log);
		return 0;
	}

	return shader;
}

GLuint
compile_shader_program(const char *vs_filename,
                       const char *gs_filename,
                       const char *fs_filename)
{
	GLuint program = glCreateProgram();

	if (vs_filename) {
		GLuint vs = compile_shader_file(vs_filename, GL_VERTEX_SHADER);
		if (vs == 0)
			goto shader_error;
		glAttachShader(program, vs);
	}
	if (gs_filename) {
		GLuint gs = compile_shader_file(gs_filename, GL_GEOMETRY_SHADER);
		if (gs == 0)
			goto shader_error;
		glAttachShader(program, gs);
	}
	if (fs_filename) {
		GLuint fs = compile_shader_file(fs_filename, GL_FRAGMENT_SHADER);
		if (fs == 0)
			goto shader_error;
		glAttachShader(program, fs);
	}

	glLinkProgram(program);

	GLint result;
	glGetProgramiv(program, GL_LINK_STATUS, &result);
	if (result == GL_FALSE) {
		char log[1024];
		glGetProgramInfoLog(program, 1023, NULL, log);
		errno = EINVAL;
		xperror("Error linking shader program");
		fprintf(stderr, "%s\n", log);
		return 0;
	}

	/* TODO: Detach/delete shaders? */

	return program;

shader_error:
	glDeleteProgram(program);
	return 0;
}
