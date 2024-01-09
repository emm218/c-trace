#ifndef TOKEN_H
#define TOKEN_H

#include "scene.h"

typedef enum {
	CAMERA,
	LIGHT,
	MATERIAL,
} keyword;

typedef struct {
	enum {
		LPAREN,
		RPAREN,
		KEYWORD,
		SHAPE_TYPE,
		LIGHT_TYPE,
		MATERIAL_TYPE,
		COLOR_TYPE,
		NUMBER,
		END,
		ERROR,
	} type;
	union {
		float f;
		keyword k;
		shape_type s;
		light_type l;
		color_type c;
		material_type m;
	};
} token;

struct keyword_set {
	char *name;
	token token;
};

struct keyword_set *get_keyword(register const char *str, register size_t len);

#endif /* TOKEN_H */
