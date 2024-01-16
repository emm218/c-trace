#include <math.h>
#include <stdio.h>

#include "texture.h"

static color
sample_pixel(texture *tex, int x, int y)
{
	int n, offset;
	float *img;

	n = tex->image.n_channels;
	offset = (y * tex->image.width + x) * n;
	img = tex->image.data;

	if (n < 3) {
		return (color) { img[offset], img[offset], img[offset] };
	}
	return (color) { img[offset], img[offset + 1], img[offset + 2] };
}

static color
sample_image(texture *tex, float u, float v)
{
	float xf, yf, a, b;
	int xi, yi;
	color up_left, down_left, up_right, down_right, up, down;

	xf = u * (tex->image.width - 1);
	yf = v * (tex->image.height - 1);
	a = xf - floorf(xf);
	b = yf - floorf(yf);

	xi = xf;
	yi = yf;

	up_left = sample_pixel(tex, xi, yi);
	down_left = sample_pixel(tex, xi, yi + 1);
	up_right = sample_pixel(tex, xi + 1, yi);
	down_right = sample_pixel(tex, xi + 1, yi + 1);

	up = color_lerp(up_left, up_right, a);
	down = color_lerp(down_left, down_right, a);

	return color_lerp(up, down, b);
}

color
sample_texture(texture *tex, float u, float v)
{
	int x, y;

	switch (tex->type) {
	case SOLID:
		return tex->solid;
	case IMAGE:
		return sample_image(tex, u, v);
	case CHECKS:
		x = tex->checks.scale * 2 * u;
		y = tex->checks.scale * 2 * v;
		return (x + y) % 2 ? tex->checks.c1 : tex->checks.c2;
	}
}
