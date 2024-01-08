#ifndef SCENE_H
#define SCENE_H

#include <cglm/cglm.h>
#include <stdio.h>

typedef vec4 vec;

typedef struct {
	float r, g, b;
} color;

typedef struct {
	char *meow;
} material;

typedef struct {
	vec eye, down, right, upper_left;
} camera;

typedef enum {
	AMBIENT,
	DIRECTIONAL,
	POINT,
} light_type;

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
	shape_type type;
	material *material;
	char data[];
} shape;

typedef struct {
	vec normal;
	float d;
} plane;

typedef struct {
	vec center;
	float r;
} sphere;

typedef struct {
	vec normal;
	material *material;
	float t;
} hit_info;

struct scene {
	camera camera;
};

extern struct scene scene;

int load_scene(FILE *, float);

#endif /* SCENE_H */
