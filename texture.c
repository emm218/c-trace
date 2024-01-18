#include <cglm/cglm.h>
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
	int xi, yi, w, h;
	color up_left, down_left, up_right, down_right, up, down;

	w = tex->image.width;
	h = tex->image.height;

	xf = u * w;
	yf = v * h;
	a = xf - floorf(xf);
	b = yf - floorf(yf);

	xi = xf;
	yi = yf;

	up_left = sample_pixel(tex, xi, yi);
	down_left = sample_pixel(tex, xi, (yi + 1) % h);
	up_right = sample_pixel(tex, (xi + 1) % w, yi);
	down_right = sample_pixel(tex, (xi + 1) % w, (yi + 1) % h);

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

static float
sample_pixel_intensity(texture *tex, int x, int y)
{
	int n, offset;
	float *img;

	n = tex->image.n_channels;
	offset = (y * tex->image.width + x) * n;
	img = tex->image.data;

	glm_vec3_clamp(img + offset, 0.0, 1.0);

	if (n < 3) {
		return 3 * img[offset];
	}
	return img[offset] + img[offset + 1] + img[offset + 2];
}

float
sample_intensity(texture *tex, float u, float v)
{
	int x, y;
	color c;

	switch (tex->type) {
	case SOLID:
		c = tex->solid;
		return c.r + c.g + c.b;
	case IMAGE:
		x = u * tex->image.width;
		y = v * tex->image.height;
		return sample_pixel_intensity(tex, x, y);
	case CHECKS:
		x = tex->checks.scale * 2 * u;
		y = tex->checks.scale * 2 * v;
		c = (x + y) % 2 ? tex->checks.c1 : tex->checks.c2;
		return c.r + c.g + c.b;
	}
}
