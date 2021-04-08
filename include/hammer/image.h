#ifndef HAMMER_IMAGE_H_
#define HAMMER_IMAGE_H_

#include <stddef.h>
#include <deadlock/dl.h>

/*
 * Creates a heightmap image and returns zero on success, or sets and returns
 * errno on error. Height values are normalized linearly between min and max
 * extents.
 */
int write_heightmap(
	const char  *filename,
	const float *map,
	size_t       width,
	size_t       height,
	float        min,
	float        max
);

/*
 * Creates an RGB channel image and returns zero on success, or sets and
 * returns errno on error.
 */
int write_rgb(
	const char  *filename,
	const float *img,
	size_t       width,
	size_t       height
);

#endif /* HAMMER_IMAGE_H_ */
