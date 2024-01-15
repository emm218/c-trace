#ifndef SCENE_H
#define SCENE_H

#include <cglm/cglm.h>
#include <stdio.h>

#define PRINT_VEC(s, v) fprintf(stderr, s "(%f %f %f)\n", v[0], v[1], v[2])

typedef vec4 vec;

typedef struct {
	float r, g, b;
} __attribute__((aligned(16))) color;

typedef struct {
	// void (*scatter)(const ray *, ray *);
} material;

typedef struct {
	vec eye, down, right, upper_left;
} camera;

typedef enum {
	AMBIENT,
	DIRECTIONAL,
	POINT,
} light_type;

typedef struct {
	light_type type;
	color color;
	vec data;
} light;

typedef enum {
	RGB,
	BLACKBODY,
} color_type;

typedef enum {
	DIFFUSE,
	DIELECTRIC,
	EMISSIVE,
} material_type;

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
	material *material;
	float t;
} hit_info;

struct scene {
	camera camera;
	color ambient_light;
	light lights[4];
	shape shapes[4];
	size_t cur_light, cur_shape;
};

extern struct scene scene;

extern const float epsilon;

int load_scene(FILE *, float);
int hit_sphere(const sphere *, const ray *, hit_info *);
int hit_plane(const plane *, const ray *, hit_info *);
int hit_scene(const ray *, hit_info *);

#endif /* SCENE_H */
