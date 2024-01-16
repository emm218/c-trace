#ifndef SCENE_H
#define SCENE_H

#include <cglm/cglm.h>
#include <stdio.h>

#include "color.h"
#include "texture.h"

#define PRINT_VEC(s, v) fprintf(stderr, s "(%f %f %f)\n", v[0], v[1], v[2])

typedef vec4 vec;

typedef struct {
	vec eye, down, right, upper_left;
} camera;

typedef enum {
	DIFFUSE,
	SPECULAR,
	EMISSIVE,
} material_type;

typedef struct material_s {
	struct material_s *next;
	material_type type;
	texture texture;
} material;

typedef enum {
	PLANE,
	SPHERE,
} shape_type;

typedef struct {
	vec normal;
	vec center;
	float d;
} plane;

typedef struct {
	vec center;
	float r;
} sphere;

typedef struct {
	shape_type type;
	material *material;
	union {
		plane p;
		sphere s;
	};
} shape;

typedef struct {
	vec d;
	vec origin;
} ray;

typedef struct {
	vec normal;
	vec p;
	float u, v;
	material *material;
	float t;
} hit_info;

struct scene {
	camera camera;
	texture background;
	shape shapes[4];
	material *materials;
	size_t cur_shape;
};

extern struct scene scene;

extern const float epsilon;

int load_scene(FILE *, float);
int hit_sphere(const sphere *, const ray *, hit_info *);
int hit_plane(const plane *, const ray *, hit_info *);
int hit_scene(const ray *, hit_info *);

#endif /* SCENE_H */
