#ifndef COLOR_H
#define COLOR_H

typedef struct {
	float r, g, b;
} __attribute__((aligned(16))) color;

void color_add(color *, color);
void color_mul(color *, color);

#endif /* COLOR_H */
