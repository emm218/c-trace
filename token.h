#ifndef TOKEN_H
#define TOKEN_H

#include "scene.h"

typedef enum {
	CAMERA,
	BACKGROUND,
	MATERIAL,
	BLACKBODY,
	KW_CHECKS,
} keyword;

typedef struct {
	enum {
		LPAREN,
		RPAREN,
		KEYWORD,
		SHAPE_TYPE,
		MATERIAL_TYPE,
		NUMBER,
		STRING,
		END,
		ERROR,
	} type;
	union {
		float f;
		keyword k;
		shape_type s;
		material_type m;
		char *str;
	};
} token;

struct keyword_set {
	char *name;
	token token;
};

struct keyword_set *get_keyword(register const char *str, register size_t len);

#endif /* TOKEN_H */
