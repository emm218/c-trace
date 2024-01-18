#ifndef TEXTURE_H
#define TEXTURE_H

#include <stddef.h>

#include "color.h"

typedef enum {
	SOLID,
	CHECKS,
	IMAGE,
} texture_type;

typedef struct {
	texture_type type;
	union {
		struct {
			int width, height, n_channels;
			float *data;
		} image;
		struct {
			float scale;
			color c1, c2;
		} checks;
		color solid;
	};
} texture;

color sample_texture(texture *, float, float);
float sample_intensity(texture *, float, float);

#endif /* TEXTURE_H */
