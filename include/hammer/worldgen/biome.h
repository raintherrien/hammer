#ifndef HAMMER_WORLDGEN_BIOME_H_
#define HAMMER_WORLDGEN_BIOME_H_

enum biome {
	/* Special ocean biomes */
	BIOME_OCEAN,
	BIOME_ICE_SHEET,
	/* Arctic */
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

	BIOME_COUNT
};

extern const char         *biome_name [BIOME_COUNT];
extern const unsigned char biome_color[BIOME_COUNT][3];

enum biome biome_class(float elev, float temp, float precip);
void DEBUG_print_biome_map(void);

#endif /* HAMMER_WORLDGEN_BIOME_H_ */
