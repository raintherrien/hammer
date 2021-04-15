#include "hammer/image.h"
#include "hammer/error.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <png.h>
#include <SDL2/SDL_image.h>

static void
errorfn(png_structp png_ptr, png_const_charp msg)
{
	(void) png_ptr;
	xpanicva("libpng error %s", msg);
}

static void
warnfn(png_structp png_ptr, png_const_charp msg)
{
	(void) png_ptr;
	xperrorva("libpng warning %s", msg);
}

int
write_heightmap(
	const char  *filename,
	const float *map,
	size_t       width,
	size_t       height,
	float        min,
	float        max
)
{
	uint16_t row[width];

	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
	                                              NULL, errorfn, warnfn);
	if (png_ptr == NULL) {
		xperror("png_create_write_struct returned NULL");
		goto error_creating_png_struct;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		xperror("png_create_info_struct returned NULL");
		goto error_creating_png_info;
	}

	FILE *pngfile = fopen(filename, "wb");
	if (!pngfile) {
		xperrorva("Error creating heightmap file: \"%s\"", filename);
		goto error_opening_file;
	}

	png_init_io(png_ptr, pngfile);
	png_set_IHDR(png_ptr, info_ptr, width, height, 16,
	             PNG_COLOR_TYPE_GRAY,          PNG_INTERLACE_NONE,
	             PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_set_compression_level(png_ptr, 1);
	png_write_info(png_ptr, info_ptr);
	png_set_swap(png_ptr); /* TODO: Is this valid on Big-Endian hw? */

	for (size_t y = 0; y < height; ++ y) {
		for (size_t x = 0; x < width; ++ x) {
			size_t i = y * width + x;
			row[x] = (map[i] - min) / (max - min) * (uint16_t)-1;
		}
		png_write_row(png_ptr, (png_const_bytep)row);
	}

	png_write_end(png_ptr, info_ptr);
	png_destroy_write_struct(&png_ptr, &info_ptr);
	if (fclose(pngfile) != 0) xperror("Error closing heightmap file");

	return 0;

error_opening_file:
	png_destroy_write_struct(&png_ptr, &info_ptr);
error_creating_png_info:
	png_destroy_write_struct(&png_ptr, NULL);
error_creating_png_struct:
	return errno;
}

int write_rgb(
	const char  *filename,
	const float *img,
	size_t       width,
	size_t       height
)
{
	uint8_t row[3*width];

	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
	                                              NULL, errorfn, warnfn);
	if (png_ptr == NULL) {
		xperror("png_create_write_struct returned NULL");
		goto error_creating_png_struct;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		xperror("png_create_info_struct returned NULL");
		goto error_creating_png_info;
	}

	FILE *pngfile = fopen(filename, "wb");
	if (!pngfile) {
		xperrorva("Error creating image file: \"%s\"", filename);
		goto error_opening_file;
	}

	png_init_io(png_ptr, pngfile);
	png_set_IHDR(png_ptr, info_ptr, width, height, 8,
	             PNG_COLOR_TYPE_RGB,           PNG_INTERLACE_NONE,
	             PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_set_compression_level(png_ptr, 1);
	png_write_info(png_ptr, info_ptr);
	png_set_swap(png_ptr); /* TODO: Is this valid on Big-Endian hw? */

	for (size_t y = 0; y < height; ++ y) {
		for (size_t x = 0; x < width; ++ x) {
			size_t i = y * width + x;
			row[x*3+0] = img[i*3+0] * (uint8_t)-1;
			row[x*3+1] = img[i*3+1] * (uint8_t)-1;
			row[x*3+2] = img[i*3+2] * (uint8_t)-1;
		}
		png_write_row(png_ptr, (png_const_bytep)row);
	}

	png_write_end(png_ptr, info_ptr);
	png_destroy_write_struct(&png_ptr, &info_ptr);
	if (fclose(pngfile) != 0)
		xperror("Error closing image file");

	return 0;

error_opening_file:
	png_destroy_write_struct(&png_ptr, &info_ptr);
error_creating_png_info:
	png_destroy_write_struct(&png_ptr, NULL);
error_creating_png_struct:
	return errno;
}

GLuint
load_texture(const char *filename, int *width, int *height)
{
	SDL_Surface *surface = IMG_Load(filename);
	if (!surface) {
		fprintf(stderr, ANSI_ESC_BOLDRED
		                "Error loading image file %s: %s\n"
		                ANSI_ESC_BOLDRED,
		                filename,
		                SDL_GetError());
		return 0;
	}

	GLenum format;
	GLenum internalformat;
	switch (surface->format->BytesPerPixel) {
	case 4:
		format = (SDL_BYTEORDER == SDL_BIG_ENDIAN ? GL_BGRA : GL_RGBA);
		internalformat = GL_RGBA;
		break;
	default /*3*/:
		format = (SDL_BYTEORDER == SDL_BIG_ENDIAN ? GL_BGR : GL_RGB);
		internalformat = GL_RGB;
		break;
	}

	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, /* level */
	                            internalformat,
	                            surface->w, surface->h,
	                            0, /* border */
	                            format,
	                            GL_UNSIGNED_BYTE,
	                            surface->pixels);
	if (width)
		*width = surface->w;
	if (height)
		*height = surface->h;
	SDL_FreeSurface(surface);
	return texture;
}
