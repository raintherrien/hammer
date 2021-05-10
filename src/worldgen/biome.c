#include "hammer/worldgen/biome.h"
#include "hammer/image.h"
#include "hammer/mem.h"
#include "hammer/worldgen/climate.h"
#include "hammer/worldgen/tectonic.h"
#include <stdlib.h>
#include <string.h>

const char *biome_name[BIOME_COUNT] = {
	"Ocean",
	"Ice Sheet",
	"Polar Desert",
	"Tundra",
	"Taiga",
	"Temperate Forest",
	"Temperate Grassland",
	"Temperate Desert",
	"Rainforest",
	"Savanna",
	"Desert"
};

const unsigned char biome_color[BIOME_COUNT][3] = {
	{  96, 153, 191 },
	{ 169, 197, 204 },
	{ 190, 205, 209 },
	{ 186, 175, 158 },
	{   2, 112,  83 },
	{  64, 143,  81 },
	{ 130, 143,  64 },
	{ 163, 150,  86 },
	{   2, 112,  11 },
	{ 143, 132,  64 },
	{ 222, 194, 104 }
};

enum biome
biome_class(float elev, float temp, float precip)
{
	/* Special ocean biomes, denoted by negative precip */
	if (elev < TECTONIC_CONTINENT_MASS) {
		if (temp < CLIMATE_ARCTIC_MAX_TEMP)
			return BIOME_ICE_SHEET;
		else
			return BIOME_OCEAN;
	}

	if (temp < CLIMATE_ARCTIC_MAX_TEMP) {
		if (precip < 0.3f)
			return BIOME_POLAR_DESERT;
		else
			return BIOME_TUNDRA;
	}

	if (temp < CLIMATE_SUBARCTIC_MAX_TEMP) {
		return BIOME_TAIGA;
	}

	if (temp < CLIMATE_TEMPERATE_MAX_TEMP) {
		if (precip < 0.025f)
			return BIOME_TEMPERATE_DESERT;
		else if (precip < 0.4f)
			return BIOME_TEMPERATE_GRASSLAND;
		else
			return BIOME_TEMPERATE_FOREST;
	}

	if (precip < 0.025f)
		return BIOME_DESERT;
	else if (precip < 0.7f)
		return BIOME_SAVANNA;
	else
		return BIOME_RAINFOREST;
}

void
DEBUG_print_biome_map(void)
{
	const unsigned dim = 1024;
	unsigned char *img = xmalloc(dim * dim * sizeof(*img) * 3);
	for (unsigned y = 0; y < dim; ++ y)
	for (unsigned x = 0; x < dim; ++ x) {
		size_t i = y * dim + x;
		float t = 1 - y / (float)dim;
		float p =     x / (float)dim;
		enum biome b = biome_class(TECTONIC_CONTINENT_MASS, t, p);
		memcpy(&img[i*3], biome_color[b], sizeof(unsigned char) * 3);
	}

	write_rgb("biome_map.png", img, dim, dim);
	free(img);
}
