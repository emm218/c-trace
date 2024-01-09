#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "scene.h"
#include "token.h"

static int parse_camera(camera *, float);
static int parse_light(void);
static int parse_color(color *);
static int parse_vec(vec);

#define TODO()                      \
	fprintf(stderr, "todo!\n"); \
	return 1;

#define BUF_SIZE 4096

static const char *token_types[] = {
	"'('",
	"')'",
	"KEYWORD",
	"a shape type",
	"a light type",
	"a material type",
	"a color type",
	"NUMBER",
	"EOF",
	"ERROR",
};

extern char *prog_name;

static char buf[BUF_SIZE], *tok, *cur, *lim;
static int eof;
static size_t line;
static FILE *input;
static token t;

struct scene scene;

static int
fill(void)
{
	size_t free, n;

	memmove(buf, tok, cur - tok);
	cur -= tok - buf;
	tok = buf;
	free = sizeof(buf) - (cur - tok);
	if (free <= 0) {
		fprintf(stderr,
		    "%s: token '%s' on line %zu exceeded max length (%u)\n",
		    prog_name, cur, line, BUF_SIZE);
		return 1;
	}

	n = fread(cur, sizeof(char), free, input);
	if (n < free && ferror(input)) {
		fprintf(stderr, "%s: error reading input file\n", prog_name);
		return 1;
	}

	lim = cur + n;

	eof = feof(input);

	return 0;
}

static int
isnumeric(char c)
{
	return isdigit(c) || c == '.' || c == ',' || c == 'e' || c == '-' ||
	    c == '+';
}

#define CONSUME_WHILE(p)                                  \
	while (p) {                                       \
		if (cur == lim) {                         \
			if (eof)                          \
				break;                    \
			if (fill() != 0)                  \
				return (token) { ERROR }; \
		}                                         \
		cur++;                                    \
	}

static token
next_token()
{
	char *end;
	token ret;
	struct keyword_set *kw;
loop:
	if (cur == lim) {
		if (eof) {
			return (token) { END };
		}
		if (fill() != 0) {
			return (token) { ERROR };
		}
	}

	switch (*cur++) {
	case '#':
		CONSUME_WHILE(*cur != '\n');
		cur++;
	case '\n':
		line++;
	case ' ':
	case '\t':
		goto loop;
	case '(':
		return (token) { LPAREN };
	case ')':
		return (token) { RPAREN };
	case '-':
	case '0' ... '9':
		tok = cur - 1;
		CONSUME_WHILE(isnumeric(*cur));
		ret = (token) { NUMBER };
		errno = 0;
		ret.f = strtof(tok, &end);
		if (errno) {
			fprintf(stderr, "%s: error on line %zu: %s\n",
			    prog_name, line, strerror(errno));
			return (token) { ERROR };
		} else if (end != cur) {
			*cur = 0;
			fprintf(stderr, "%s: invalid number '%s' on line %zu\n",
			    prog_name, tok, line);
			return (token) { ERROR };
		}
		return ret;
	case 'a' ... 'z':
	case 'A' ... 'Z':
		tok = cur - 1;
		CONSUME_WHILE(isalpha(*cur));
		if ((kw = get_keyword(tok, cur - tok)))
			return kw->token;
		*cur = 0;
		fprintf(stderr, "%s: unknown token '%s' on line %zu\n",
		    prog_name, tok, line);
	}

	return (token) { ERROR };
}

#define CONSUME(ty)                                                        \
	if ((t = next_token()).type != (ty)) {                             \
		*cur = 0;                                                  \
		fprintf(stderr, "%s: expected %s on line %zu, got '%s'\n", \
		    prog_name, token_types[ty], line, tok);                \
		return 1;                                                  \
	}

#define PARSE(f, ...)                      \
	if (parse_##f(__VA_ARGS__) != 0) { \
		return 1;                  \
	}

#define CONSUME_FLOAT()                                              \
	(t = next_token()).f;                                        \
	if (t.type != NUMBER) {                                      \
		*cur = 0;                                            \
		fprintf(stderr,                                      \
		    "%s: on line %zu expected a number, got '%s'\n", \
		    prog_name, line, tok);                           \
		return 1;                                            \
	}
int
load_scene(FILE *in, float aspect_ratio)
{
	tok = cur = lim = buf;
	eof = 0;
	line = 1;

	input = in;

	scene.ambient_light.r = INFINITY;
	scene.ambient_light.g = INFINITY;
	scene.ambient_light.b = INFINITY;

	scene.cur_light = 0;
	scene.cur_shape = 0;
loop:
	switch ((t = next_token()).type) {
	case KEYWORD:
		switch (t.k) {
		case CAMERA:
			PARSE(camera, &scene.camera, aspect_ratio);
			break;
		case LIGHT:
			PARSE(light);
			break;
		case MATERIAL:
			// TODO: actual materials handling
			CONSUME(MATERIAL_TYPE);
			break;
		}
		break;
	case SHAPE_TYPE:
		switch (t.s) {
		case PLANE:
			PARSE(vec, scene.shapes[scene.cur_shape].p.normal);
			scene.shapes[scene.cur_shape].p.d = CONSUME_FLOAT();
			break;
		case SPHERE:
			PARSE(vec, scene.shapes[scene.cur_shape].s.center);
			scene.shapes[scene.cur_shape].s.r = CONSUME_FLOAT();
			break;
		}
		scene.cur_shape++;
		break;
	default:
		*cur = 0;
		fprintf(stderr,
		    "%s: on line %zu expected a keyword"
		    " (light, material, camera) or a shape type, got '%s'\n",
		    prog_name, line, tok);
		return 1;
	case END:
		return 0;
	}

	goto loop;
}

static int
parse_camera(camera *out, float aspect_ratio)
{
	vec look, look_at, up;
	float fov, vw, vh;

	PARSE(vec, out->eye);
	PARSE(vec, look_at);
	PARSE(vec, up);
	fov = CONSUME_FLOAT();
	fov *= GLM_PI / 180.0;

	glm_vec4_sub(look_at, out->eye, look);

	vw = fabsf(atanf(fov / 2.0)) * glm_vec4_norm(look) * 2.0;
	vh = vw / aspect_ratio;

	glm_vec3_proj(up, look, out->down);
	glm_vec4_sub(out->down, up, out->down);
	glm_vec4_scale_as(out->down, vh, out->down);

	glm_vec3_cross(look, out->down, out->right);
	glm_vec4_scale_as(out->right, vw, out->right);

	return 0;
}

static int
parse_light()
{
	CONSUME(LIGHT_TYPE);
	switch (t.l) {
	case AMBIENT:
		if (scene.ambient_light.r != INFINITY) {
			fprintf(stderr,
			    "%s: second ambient light declaration on line %zu\n",
			    prog_name, line);
			return 1;
		}
		PARSE(color, &scene.ambient_light);
		break;
	case DIRECTIONAL:
	case POINT:
		scene.lights[scene.cur_light].type = t.l;
		PARSE(vec, scene.lights[scene.cur_light].data);
		PARSE(color, &scene.lights[scene.cur_light].color);
		scene.cur_light++;
		break;
	}
	return 0;
}

static int
parse_color(color *out)
{
	CONSUME(COLOR_TYPE);
	switch (t.c) {
	case RGB:
		out->r = CONSUME_FLOAT();
		out->g = CONSUME_FLOAT();
		out->b = CONSUME_FLOAT();
		break;
	case BLACKBODY:
		TODO();
	}
	return 0;
}

static int
parse_vec(vec out)
{
	CONSUME(LPAREN);
	out[0] = CONSUME_FLOAT();
	out[1] = CONSUME_FLOAT();
	out[2] = CONSUME_FLOAT();
	CONSUME(RPAREN);
	return 0;
}
