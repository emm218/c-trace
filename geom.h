#ifndef GEOM_H
#define GEOM_H

#include <cglm/cglm.h>

#include "material.h"

typedef vec4 vec;

typedef enum {
	PLANE,
	SPHERE,
} shape_type;

typedef struct {
	vec normal;
	vec center;
	vec u, v;
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

extern const float epsilon;

int hit_sphere(const sphere *, const ray *, hit_info *);
int hit_plane(const plane *, const ray *, hit_info *);

#endif /* GEOM_H */
