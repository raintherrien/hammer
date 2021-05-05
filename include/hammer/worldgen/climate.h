#ifndef HAMMER_WORLDGEN_CLIMATE_H_
#define HAMMER_WORLDGEN_CLIMATE_H_

/* Climate map dimensions (same as lithosphere) */
#define CLIMATE_SCALE 10
#define CLIMATE_LEN (1<<CLIMATE_SCALE)
#define CLIMATE_AREA (CLIMATE_LEN * CLIMATE_LEN)

#define CLIMATE_GENERATIONS 250
#define CLIMATE_ARCTIC_MAX_TEMP 0.15f
#define CLIMATE_SUBARCTIC_MAX_TEMP 0.40f
#define CLIMATE_TEMPERATE_MAX_TEMP 0.65f

struct lithosphere;

/*
 * TODO: Document
 */
struct climate {
	float uplift       [CLIMATE_AREA];
	float moisture     [CLIMATE_AREA];
	float precipitation[CLIMATE_AREA];
	float inv_temp_init[CLIMATE_AREA];
	float inv_temp     [CLIMATE_AREA];
	float inv_temp_flow[CLIMATE_AREA * 4];
	float wind_velocity[CLIMATE_AREA * 2];
	unsigned long generation;
};

void climate_create(struct climate *, struct lithosphere *);
void climate_destroy(struct climate *);
void climate_update(struct climate *);

#endif /* HAMMER_WORLDGEN_CLIMATE_H_ */
