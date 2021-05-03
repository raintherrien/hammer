#ifndef HAMMER_WORLDGEN_CLIMATE_H_
#define HAMMER_WORLDGEN_CLIMATE_H_

#define CLIMATE_GENERATIONS 250
#define CLIMATE_ARCTIC_MAX_TEMP 0.15f
#define CLIMATE_SUBARCTIC_MAX_TEMP 0.40f
#define CLIMATE_TEMPERATE_MAX_TEMP 0.65f

/*
 * TODO: Document
 */
struct climate {
	/* Non-owning */
	const float * restrict uplift;
	/* Following allocated by climate_create, freed by climate_destroy */
	float * restrict moisture;
	float * restrict precipitation;
	float * restrict inv_temp_init;
	float * restrict inv_temp;
	float * restrict inv_temp_flow; /* Array of float[4] vectors. */
	float * restrict wind_velocity; /* Array of float[2] vectors. */
	unsigned long size;
	unsigned long generation;
};

void climate_create(struct climate *, const float *uplift, unsigned long size);
void climate_destroy(struct climate *);
void climate_update(struct climate *);

#endif /* HAMMER_WORLDGEN_CLIMATE_H_ */
