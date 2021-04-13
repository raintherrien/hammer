#ifndef HAMMER_WORLDGEN_CLIMATE_H_
#define HAMMER_WORLDGEN_CLIMATE_H_

/* TODO: Document */
struct climate {
	float * restrict moisture;
	float * restrict precipitation;
	float * restrict pressure;
	float * restrict pressure_flow; /* Array of float[4] vectors. */
	float * restrict temperature;
	float * restrict wind_velocity; /* Array of float[2] vectors. */
};

struct climate generate_climate(const float *uplift, unsigned long size);

#endif /* HAMMER_WORLDGEN_CLIMATE_H_ */
