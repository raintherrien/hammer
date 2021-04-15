#include "hammer/file.h"
#include "hammer/error.h"
#include "hammer/mem.h"
#include <stdio.h>

char *
read_file(const char *filename)
{
	FILE *f = fopen(filename, "rb");
	if (!f) {
		xperrorva("Error opening file %s\n", filename);
		return NULL;
	}

	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (size <= 0)
		goto error_reading_file;
	char *str = xmalloc(size + 1);
	if (fread(str, 1, size, f) != (size_t)size)
		goto error_reading_file;
	str[size] = '\0';

	fclose(f);
	return str;

error_reading_file:
	xperrorva("Error reading file %s\n", filename);
	fclose(f);
	return NULL;
}
