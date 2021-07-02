#ifndef HAMMER_CLIENT_GLSL_H_
#define HAMMER_CLIENT_GLSL_H_

#include <GL/glew.h>

GLuint compile_shader_file(const char *filename, GLenum shader_type);
GLuint compile_shader_program(const char *vs_filename,
                              const char *gs_filename,
                              const char *fs_filename);

#endif /* HAMMER_CLIENT_GLSL_H_ */
