#include "hammer/math.h"
#include "hammer/mem.h"
#include "hammer/worldgen/climate.h"
#include "hammer/worldgen/tectonic.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

/*
 * TODO: Document
 * Pressure fluid model derived from shallow-water model of:
 *   Balázs Jákó and Balázs Tóth, Fast Hydraulic and Thermal Erosion on the
 *   GPU, 2011.
 */

#define RATE_OF_EVAPORATION   0.25f
#define RATE_OF_PRECIPITATION 0.001f
#define RATE_OF_FLOW          0.25f

static void advection(struct climate *, unsigned long size);
static void equalize_pressure(struct climate *, unsigned long size);
static float latitude_temperature(float latitude);
static void precipitation(struct climate *, const float *uplift, unsigned long size);
static void temperature(struct climate *, const float *uplift, unsigned long size);

#ifdef HAMMER_DEBUG_CLIMATE
#include "hammer/image.h"
static void debug_print_elevation(const float *uplift, unsigned long size);
static void debug_print_moisture(const float *moisture, unsigned long size, int generation);
static void debug_print_precipitation(const float *uplift, const float *precip, unsigned long size);
static void debug_print_pressure(const float *pressure, unsigned long size, int generation);
static void debug_print_temperature(const float *temp, unsigned long size);
#endif

static inline size_t
wrap(long long x, unsigned long size)
{
	return (x + size) % size;
}

struct climate
generate_climate(const float *uplift, unsigned long size)
{
	struct climate c = {
		.moisture      = xcalloc(size * size, sizeof(float)),
		.precipitation = xcalloc(size * size, sizeof(float)),
		.pressure      = xcalloc(size * size, sizeof(float)),
		.pressure_flow = xcalloc(size * size * 4, sizeof(float)),
		.temperature   = xcalloc(size * size, sizeof(float)),
		.wind_velocity = xcalloc(size * size * 2, sizeof(float))
	};

	temperature(&c, uplift, size);

	for (size_t y = 0; y < size; ++ y)
	for (size_t x = 0; x < size; ++ x) {
		size_t i = y * size + x;
		c.pressure[i] = 1 - c.temperature[i];
		c.pressure[i] += powf(fabsf(2.0f * y / size - 1), 2);
	}

	for (int generation = 0; generation < 250; ++ generation) {
		precipitation(&c, uplift, size);
		equalize_pressure(&c, size);
		advection(&c, size);
		printf("Climate generation %d/250\n", generation);
#ifdef HAMMER_DEBUG_CLIMATE
		debug_print_pressure(c.pressure, size, generation);
		debug_print_moisture(c.moisture, size, generation);
#endif
	}

#ifdef HAMMER_DEBUG_CLIMATE
	debug_print_elevation(uplift, size);
	debug_print_precipitation(uplift, c.precipitation, size);
	debug_print_temperature(c.temperature, size);
#endif

	return c;
}

static float
lerp_moisture(struct climate *c, unsigned long size, float x, float y)
{
	float fx = floorf(x);
	float fy = floorf(y);
	float cx = ceilf(x);
	float cy = ceilf(y);
	float m00 = c->moisture[wrap(fy, size) * size + wrap(fx, size)];
	float m10 = c->moisture[wrap(fy, size) * size + wrap(cx, size)];
	float m01 = c->moisture[wrap(cy, size) * size + wrap(fx, size)];
	float m11 = c->moisture[wrap(cy, size) * size + wrap(cx, size)];
	float m0 = m00 + (m10 - m00) * (x - fx);
	float m1 = m01 + (m11 - m01) * (x - fx);
	return m0 + (m1 - m0) * (y - fy);
}

static void
advection(struct climate *c, unsigned long size)
{
	/* Note: Uses wind_velocity as scratch */
	for (size_t y = 0; y < size; ++ y)
	for (size_t x = 0; x < size; ++ x) {
		size_t i = y * size + x;
		float u = x - (1/RATE_OF_FLOW)*c->wind_velocity[i*2+0];
		float v = y - (1/RATE_OF_FLOW)*c->wind_velocity[i*2+1];
		c->wind_velocity[i*2+0] = lerp_moisture(c, size, u, v);
	}

	for (size_t i = 0; i < size * size; ++ i)
		c->moisture[i] = c->wind_velocity[i*2+0];
}

static void
equalize_pressure(struct climate *c, unsigned long size)
{
	/* Calculate outflow */
	for (size_t y = 0; y < size; ++ y)
	for (size_t x = 0; x < size; ++ x) {
		float trade_wind_bias = cosf(M_PI * (2.0f * y / size - 1));
		size_t i = y * size + x;
		float p = c->pressure[i];
		float d[4] = {
			p - c->pressure[y * size + wrap((long long)x-1, size)],
			p - c->pressure[y * size + wrap((long long)x+1, size)],
			p - c->pressure[wrap((long long)y-1, size) * size + x],
			p - c->pressure[wrap((long long)y+1, size) * size + x]
		};
		float *out = c->pressure_flow + i*4;
		float outflow = 0;
		for (size_t a = 0; a < 4; ++ a) {
			float flow = RATE_OF_FLOW * d[a];
			/* Disproportionately weight with trade wind */
			if (a == 0)
				flow *= 1 - trade_wind_bias;
			else if (a == 1)
				flow *= trade_wind_bias;
			flow = MAX(flow, 0);
			out[a] += flow;
			outflow += out[a];
		}
		if (outflow != 0 && outflow > p) {
			float s = p / outflow;
			for (size_t a = 0; a < 4; ++ a)
				out[a] *= s;
		}
	}

	/* Perform flow, velocity */
	for (size_t y = 0; y < size; ++ y)
	for (size_t x = 0; x < size; ++ x) {
		size_t i = y * size + x;
		float *out = c->pressure_flow + i*4;
		c->pressure[i] -= out[0] + out[1] + out[2] + out[3];
		size_t n[4] = {
			y * size + wrap((long long)x-1, size),
			y * size + wrap((long long)x+1, size),
			wrap((long long)y-1, size) * size + x,
			wrap((long long)y+1, size) * size + x
		};
		for (size_t a = 0; a < 4; ++ a)
			c->pressure[n[a]] += out[a];
		float in[4] = {
			c->pressure_flow[n[0]*4+1],
			c->pressure_flow[n[1]*4+0],
			c->pressure_flow[n[2]*4+3],
			c->pressure_flow[n[3]*4+2]
		};
		/* Central difference gives us velocity */
		c->wind_velocity[2*i+0] = 0.5f * (in[0] - out[0] + out[1] - in[1]);
		c->wind_velocity[2*i+1] = 0.5f * (in[2] - out[2] + out[3] - in[3]);
	}
}

static float
latitude_temperature(float latitude)
{
	return 1 - fabsf(latitude);
}

static void
precipitation(struct climate *c, const float *uplift, unsigned long size)
{
	for (size_t y = 0; y < size; ++ y)
	for (size_t x = 0; x < size; ++ x) {
		size_t i = y * size + x;
		/* Either evaporate over water or precipitate over land */
		if (uplift[i] < TECTONIC_CONTINENT_MASS) {
			float t = latitude_temperature(2.0f * y / size - 1);
			c->moisture[i] += RATE_OF_EVAPORATION * t;
		} else {
			float precip = c->moisture[i] * RATE_OF_PRECIPITATION;
			precip *= uplift[i];
			precip = MIN(c->moisture[i], MIN(precip, 1));
			c->moisture[i] -= precip;
			c->precipitation[i] += precip;
		}
	}
}

static void
temperature(struct climate *c, const float *uplift, unsigned long size)
{
	/* Calculate temperature from latitude and altitude */
	for (size_t y = 0; y < size; ++ y)
	for (size_t x = 0; x < size; ++ x) {
		size_t i = y * size + x;
		float temp = latitude_temperature(2.0f * y / size - 1);
		float h = uplift[i] - TECTONIC_CONTINENT_MASS;
		if (h < 0)
			temp *= 0.5f - (h*h);
		else
			temp *= 1 - powf(h / 2.0f, 2);
		temp = CLAMP(temp, 0, 1);
		c->temperature[i] = temp;
	}

	const int gkl = 16 * MAX(size / 1024, 1);
	const int gksz = 2*gkl+1;
	float gk[gksz];
	GAUSSIANK(gk, gksz);
	float *blur_line = xmalloc(size * 2 * sizeof(*blur_line));

	/* Blur temperature */
	for (size_t y = 0; y < size; ++ y) {
		for (size_t x = 0; x < size; ++ x) {
			blur_line[x] = 0;
			for (int g = 0; g < gksz; ++ g) {
				size_t i = y * size + wrap((long long)x+g-gkl, size);
				blur_line[x] += gk[g] * c->temperature[i];
			}
		}
		for (size_t x = 0; x < size; ++ x)
			c->temperature[y * size + x] = blur_line[x];
	}
	for (size_t x = 0; x < size; ++ x) {
		for (size_t y = 0; y < size; ++ y) {
			blur_line[y] = 0;
			for (int g = 0; g < gksz; ++ g) {
				size_t i = wrap((long long)y+g-gkl, size) * size + x;
				blur_line[y] += gk[g] * c->temperature[i];
			}
		}
		for (size_t y = 0; y < size; ++ y)
			c->temperature[y * size + x] = blur_line[y];
	}

	free(blur_line);
}

#ifdef HAMMER_DEBUG_CLIMATE
static void
debug_print_elevation(const float *uplift, unsigned long size)
{
	float *img = xmalloc(size * size * 3 * sizeof(*img));
	for (size_t i = 0; i < size * size; ++ i) {
		if (uplift[i] > TECTONIC_CONTINENT_MASS) {
			float h = uplift[i] - TECTONIC_CONTINENT_MASS;
			img[i*3+2] = 0.235f + 0.265f / 2 * MIN(2,h);
			img[i*3+1] = 0.5f;
			img[i*3+0] = 0.235f + 0.265f / 2 * MIN(2,h);
		} else {
			img[i*3+2] = uplift[i] / TECTONIC_CONTINENT_MASS;
			img[i*3+1] = 0;
			img[i*3+0] = 0;
		}
	}
	write_rgb("elevation.png", img, size, size);
	free(img);
}

static void
debug_print_moisture(const float *moisture, unsigned long size, int generation)
{
	float max = 0;
	for (size_t i = 0; i < size * size; ++ i)
		max = MAX(max, moisture[i]);
	char filename[32];
	sprintf(filename, "moisture%03d.png", generation);
	write_heightmap(filename, moisture, size, size, 0, max);
}

static void
debug_print_precipitation(const float *uplift, const float *precip, unsigned long size)
{
	float *img = xcalloc(size * size * 3, sizeof(*img));
	float max = 0;
	for (size_t i = 0; i < size * size; ++ i)
		max = MAX(max, precip[i]);
	max /= 3.0f;
	float max_color = 0.6588f;
	float min_color = 0.1960f;
	for (size_t i = 0; i < size * size; ++ i) {
		if (uplift[i] < TECTONIC_CONTINENT_MASS)
			continue;
		img[i*3+0] = precip[i] < 1 ? 1 : CLAMP(2-precip[i],0,1);
		img[i*3+1] = precip[i] < 1 ? CLAMP(precip[i],0,1) : (precip[i] > 3 ? CLAMP(4-precip[i],0,1) : 1);
		img[i*3+2] = precip[i] < 2 ? 0 : CLAMP(precip[i]-2,0,1);
		img[i*3+0] = min_color + (max_color - min_color) * img[i*3+0];
		img[i*3+1] = min_color + (max_color - min_color) * img[i*3+1];
		img[i*3+2] = min_color + (max_color - min_color) * img[i*3+2];
	}
	write_rgb("precipitation.png", img, size, size);
	free(img);
}

static void
debug_print_pressure(const float *pressure, unsigned long size, int generation)
{
	float max = 0;
	for (size_t i = 0; i < size * size; ++ i)
		max = MAX(max, pressure[i]);
	char filename[32];
	sprintf(filename, "pressure%03d.png", generation);
	write_heightmap(filename, pressure, size, size, 0, max);
}

static void
debug_print_temperature(const float *temp, unsigned long size)
{
	write_heightmap("temperature.png", temp, size, size, 0, 1);
}
#endif
