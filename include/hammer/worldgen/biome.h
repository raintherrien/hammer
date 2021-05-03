#ifndef HAMMER_WORLDGEN_BIOME_H_
#define HAMMER_WORLDGEN_BIOME_H_

#include "hammer/worldgen/climate.h"

enum biome {
	/* Arctic */
	BIOME_ICE_SHEET,
	BIOME_POLAR_DESERT,
	BIOME_TUNDRA,
	/* Sub-arctic */
	BIOME_TAIGA,
	/* Temperate */
	BIOME_TEMPERATE_FOREST,
	BIOME_TEMPERATE_GRASSLAND,
	BIOME_TEMPERATE_DESERT,
	/* Tropical */
	BIOME_RAINFOREST,
	BIOME_SAVANNA,
	BIOME_DESERT,

	BIOME_OCEAN,

	BIOME_COUNT
};

const char *biome_name[BIOME_COUNT] = {
	"Ice Sheet",
	"Polar Desert",
	"Tundra",
	"Taiga",
	"Temperate Forest",
	"Temperate Grassland",
	"Temperate Desert",
	"Rainforest",
	"Savanna",
	"Desert",
	"Ocean"
};

unsigned char biome_color[BIOME_COUNT][3] = {
	{ 169, 197, 204 },
	{ 190, 205, 209 },
	{ 186, 175, 158 },
	{   2, 112,  83 },
	{  64, 143,  81 },
	{ 130, 143,  64 },
	{ 163, 150,  86 },
	{   2, 112,  11 },
	{ 143, 132,  64 },
	{ 222, 194, 104 },
	{  96, 153, 191 }
};

static enum biome
biome_classification(float temp, float precip)
{
	/* Special ocean biomes, denoted by negative precip */
	if (precip < 0) {
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

#endif /* HAMMER_WORLDGEN_BIOME_H_ */
