#ifndef MATERIAL_H
#define MATERIAL_H

#include "texture.h"

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

#endif /* MATERIAL_H */
