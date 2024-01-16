#include "color.h"

void
color_add(color *a, color b)
{
	a->r += b.r;
	a->g += b.g;
	a->b += b.b;
}

void
color_mul(color *a, color b)
{
	a->r *= b.r;
	a->g *= b.g;
	a->b *= b.b;
}

void
color_muls(color *a, float s)
{
	a->r *= s;
	a->g *= s;
	a->b *= s;
}

color
color_lerp(color a, color b, float f)
{
	return (color) {
		a.r * (1 - f) + b.r * f,
		a.g * (1 - f) + b.g * f,
		a.b * (1 - f) + b.b * f,
	};
}
