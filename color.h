#ifndef COLOR_H
#define COLOR_H

typedef struct {
	float r, g, b;
} __attribute__((aligned(16))) color;

void color_add(color *, color);
void color_mul(color *, color);
void color_muls(color *, float);
color color_lerp(color, color, float);

#endif /* COLOR_H */
