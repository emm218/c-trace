#ifndef SCENE_H
#define SCENE_H

#include <cglm/cglm.h>
#include <stdio.h>

#include "color.h"
#include "geom.h"
#include "material.h"
#include "texture.h"

#define PRINT_VEC(s, v) fprintf(stderr, s "(%f %f %f)\n", v[0], v[1], v[2])

typedef struct {
	vec eye, down, right, upper_left;
} camera;

struct scene {
	camera camera;
	texture background;
	shape shapes[4];
	material *materials;
	size_t cur_shape;
};

extern struct scene scene;

int load_scene(FILE *, float);
int hit_scene(const ray *, hit_info *);

#endif /* SCENE_H */
