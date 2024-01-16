#include <stdio.h>

#include "texture.h"

color
sample_texture(texture *tex, float u, float v)
{
	int x, y, n, o;
	float *img;

	switch (tex->type) {
	case SOLID:
		return tex->solid;
	case IMAGE:
		x = u * tex->image.width;
		y = v * tex->image.height;
		n = tex->image.n_channels;
		img = tex->image.data;
		o = (y * tex->image.width + x) * n;
		return (color) { img[o], img[o + 1], img[o + 2] };
	case CHECKS:
		x = tex->checks.scale * 2 * u;
		y = tex->checks.scale * 2 * v;
		return (x + y) % 2 ? tex->checks.c1 : tex->checks.c2;
	}
}
