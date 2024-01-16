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
