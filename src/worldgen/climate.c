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
#define RATE_OF_PRECIPITATION 0.003f
#define RATE_OF_FLOW          0.15f
#define RATE_OF_TEMP_XFER     0.02f

static void advection(struct climate *);
static void equalize_temperature(struct climate *);
static float latitude_temperature(float latitude);
static void precipitation(struct climate *);
static void temperature_init(struct climate *);
static void temperature_update(struct climate *);

static inline size_t
wrap(long long x, unsigned long size)
{
	return (x + size) % size;
}

void
climate_create(struct climate *c, const float *uplift, unsigned long size)
{
	*c = (struct climate){
		.uplift        = uplift,
		.moisture      = xcalloc(size * size, sizeof(float)),
		.precipitation = xcalloc(size * size, sizeof(float)),
		.inv_temp_init = xcalloc(size * size, sizeof(float)),
		.inv_temp      = xcalloc(size * size, sizeof(float)),
		.inv_temp_flow = xcalloc(size * size * 4, sizeof(float)),
		.wind_velocity = xcalloc(size * size * 2, sizeof(float)),
		.size = size,
		.generation = 0
	};
	temperature_init(c);
}

void
climate_destroy(struct climate *c)
{
	free(c->moisture);
	free(c->precipitation);
	free(c->inv_temp_init);
	free(c->inv_temp);
	free(c->inv_temp_flow);
	free(c->wind_velocity);
}

void
climate_update(struct climate *c)
{
	++ c->generation;
	temperature_update(c);
	precipitation(c);
	equalize_temperature(c);
	advection(c);
}

static float
lerp(struct climate *c, float x, float y, const float *map)
{
	float fx = floorf(x);
	float fy = floorf(y);
	float cx = ceilf(x);
	float cy = ceilf(y);
	float m00 = map[wrap(fy, c->size) * c->size + wrap(fx, c->size)];
	float m10 = map[wrap(fy, c->size) * c->size + wrap(cx, c->size)];
	float m01 = map[wrap(cy, c->size) * c->size + wrap(fx, c->size)];
	float m11 = map[wrap(cy, c->size) * c->size + wrap(cx, c->size)];
	float m0 = m00 + (m10 - m00) * (x - fx);
	float m1 = m01 + (m11 - m01) * (x - fx);
	return m0 + (m1 - m0) * (y - fy);
}

static void
advection(struct climate *c)
{
	/* Note: Uses wind_velocity as scratch */
	for (size_t y = 0; y < c->size; ++ y)
	for (size_t x = 0; x < c->size; ++ x) {
		size_t i = y * c->size + x;
		float u = x - (1/RATE_OF_FLOW)*c->wind_velocity[i*2+0];
		float v = y - (1/RATE_OF_FLOW)*c->wind_velocity[i*2+1];
		c->wind_velocity[i*2+0] = lerp(c, u, v, c->moisture);
	}

	for (size_t i = 0; i < c->size * c->size; ++ i)
		c->moisture[i] = c->wind_velocity[i*2+0];
}

static void
equalize_temperature(struct climate *c)
{
	/* Calculate outflow */
	for (size_t y = 0; y < c->size; ++ y)
	for (size_t x = 0; x < c->size; ++ x) {
		float trade_wind_bias = cosf(M_PI * (2.0f * y / c->size - 1));
		size_t i = y * c->size + x;
		float t = c->inv_temp[i];
		float d[4] = {
			t - c->inv_temp[y * c->size + wrap((long long)x-1, c->size)],
			t - c->inv_temp[y * c->size + wrap((long long)x+1, c->size)],
			t - c->inv_temp[wrap((long long)y-1, c->size) * c->size + x],
			t - c->inv_temp[wrap((long long)y+1, c->size) * c->size + x]
		};
		float *out = c->inv_temp_flow + i*4;
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
		if (outflow != 0 && outflow > t) {
			float s = t / outflow;
			for (size_t a = 0; a < 4; ++ a)
				out[a] *= s;
		}
	}

	/* Perform flow, velocity */
	for (size_t y = 0; y < c->size; ++ y)
	for (size_t x = 0; x < c->size; ++ x) {
		size_t i = y * c->size + x;
		float *out = c->inv_temp_flow + i*4;
		c->inv_temp[i] -= out[0] + out[1] + out[2] + out[3];
		size_t n[4] = {
			y * c->size + wrap((long long)x-1, c->size),
			y * c->size + wrap((long long)x+1, c->size),
			wrap((long long)y-1, c->size) * c->size + x,
			wrap((long long)y+1, c->size) * c->size + x
		};
		for (size_t a = 0; a < 4; ++ a)
			c->inv_temp[n[a]] += out[a];
		float in[4] = {
			c->inv_temp_flow[n[0]*4+1],
			c->inv_temp_flow[n[1]*4+0],
			c->inv_temp_flow[n[2]*4+3],
			c->inv_temp_flow[n[3]*4+2]
		};
		/* Central difference gives us velocity */
		c->wind_velocity[2*i+0] = 0.5f * (in[0] - out[0] + out[1] - in[1]);
		c->wind_velocity[2*i+1] = 0.5f * (in[2] - out[2] + out[3] - in[3]);
	}
}

static float
latitude_temperature(float latitude)
{
	return 1 - powf(latitude, 2);
}

static void
precipitation(struct climate *c)
{
	for (size_t y = 0; y < c->size; ++ y)
	for (size_t x = 0; x < c->size; ++ x) {
		size_t i = y * c->size + x;
		/* Either evaporate over water or precipitate over land */
		if (c->uplift[i] < TECTONIC_CONTINENT_MASS) {
			c->moisture[i] += RATE_OF_EVAPORATION * (1 - c->inv_temp[i]);
		} else {
			float precip = c->moisture[i] * RATE_OF_PRECIPITATION;
			precip *= c->inv_temp[i];
			precip = MIN(c->moisture[i], MIN(precip, 1));
			c->moisture[i] -= precip;
			c->precipitation[i] += precip;
		}
	}
}

static void
temperature_init(struct climate *c)
{
	/* Calculate temperature from latitude and altitude */
	for (size_t y = 0; y < c->size; ++ y)
	for (size_t x = 0; x < c->size; ++ x) {
		size_t i = y * c->size + x;
		float temp = latitude_temperature(2.0f * y / c->size - 1);
		float h = c->uplift[i] - TECTONIC_CONTINENT_MASS;
		if (h < 0)
			temp *= 1 - (h*h);
		else
			temp *= 1 - powf(h / 3.0f, 2);
		temp = CLAMP(temp, 0, 1);
		c->inv_temp_init[i] = 1 - temp;
	}

	const int gkl = 16 * MAX(c->size / 1024, 1);
	const int gksz = 2*gkl+1;
	float gk[gksz];
	GAUSSIANK(gk, gksz);
	float *blur_line = xmalloc(c->size * 2 * sizeof(*blur_line));

	/* Blur temperature */
	for (size_t y = 0; y < c->size; ++ y) {
		for (size_t x = 0; x < c->size; ++ x) {
			blur_line[x] = 0;
			for (int g = 0; g < gksz; ++ g) {
				size_t i = y * c->size + wrap((long long)x+g-gkl, c->size);
				blur_line[x] += gk[g] * c->inv_temp_init[i];
			}
		}
		for (size_t x = 0; x < c->size; ++ x)
			c->inv_temp_init[y * c->size + x] = blur_line[x];
	}
	for (size_t x = 0; x < c->size; ++ x) {
		for (size_t y = 0; y < c->size; ++ y) {
			blur_line[y] = 0;
			for (int g = 0; g < gksz; ++ g) {
				size_t i = wrap((long long)y+g-gkl, c->size) * c->size + x;
				blur_line[y] += gk[g] * c->inv_temp_init[i];
			}
		}
		for (size_t y = 0; y < c->size; ++ y)
			c->inv_temp_init[y * c->size + x] = blur_line[y];
	}

	for (size_t i = 0; i < c->size * c->size; ++ i) {
		c->inv_temp[i] = c->inv_temp_init[i];
	}

	free(blur_line);
}

static void
temperature_update(struct climate *c)
{
	/* Lose temperature at altitude, latitude */
	for (size_t i = 0; i < c->size * c->size; ++ i) {
		if (c->inv_temp_init[i] > c->inv_temp[i]) {
			c->inv_temp[i] += (c->inv_temp_init[i] -
			                   c->inv_temp[i]) * RATE_OF_TEMP_XFER;
		}
	}
}
