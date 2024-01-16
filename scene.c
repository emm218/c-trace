#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "scene.h"
#include "token.h"

static int parse_camera(camera *, float);
static int parse_color(color *);
static int parse_material(material *);
static int parse_texture(texture *);
static int parse_vec(vec);

#define TODO()                      \
	fprintf(stderr, "todo!\n"); \
	return 1;

#define BUF_SIZE 4096

const float epsilon = 0.0001f;

static material default_material = {
	.type = DIFFUSE,
	.texture = {
		.type = SOLID,
		.solid = { 0.5, 0.5, 0.5 },
	},
};

static const char *token_types[] = {
	"'('",
	"')'",
	"KEYWORD",
	"a shape type",
	"a material type",
	"a texture type",
	"a color type",
	"a number",
	"EOF",
	"ERROR",
};

extern char *prog_name;

static char buf[BUF_SIZE], *tok, *cur, *lim;
static int eof;
static size_t line;
static FILE *input;
static token t, prev_token;

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

	if (prev_token.type != ERROR) {
		ret = prev_token;
		prev_token.type = ERROR;
		return ret;
	}

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
	case '"':
		tok = cur;
		CONSUME_WHILE(*cur != '"' && *cur != '\n');
		if (*cur == '\n') {
			fprintf(stderr, "%s: unclosed string on line %zu\n",
			    prog_name, line);
			return (token) { ERROR };
		}
		*cur = 0;
		cur++;
		return (token) { STRING, .str = tok };
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

	fprintf(stderr, "%s: unexpected character %c on line %zu\n", prog_name,
	    *(cur - 1), line);

	return (token) { ERROR };
}

#define CONSUME(ty)                                                        \
	if ((t = next_token()).type != (ty)) {                             \
		if (t.type == ERROR)                                       \
			return 1;                                          \
		*cur = 0;                                                  \
		fprintf(stderr, "%s: expected %s on line %zu, got '%s'\n", \
		    prog_name, token_types[ty], line, tok);                \
		return 1;                                                  \
	}

#define CONSUME_OPTIONAL(ty)                   \
	if ((t = next_token()).type != (ty)) { \
		if (t.type == ERROR)           \
			return 1;              \
		prev_token = t;                \
		return 0;                      \
	}

#define PARSE(f, ...)                      \
	if (parse_##f(__VA_ARGS__) != 0) { \
		return 1;                  \
	}

#define CONSUME_FLOAT()                                              \
	(t = next_token()).f;                                        \
	if (t.type != NUMBER) {                                      \
		if (t.type == ERROR)                                 \
			return 1;                                    \
		*cur = 0;                                            \
		fprintf(stderr,                                      \
		    "%s: on line %zu expected a number, got '%s'\n", \
		    prog_name, line, tok);                           \
		return 1;                                            \
	}

#define CONSUME_FLOAT_OPTIONAL()     \
	(t = next_token()).f;        \
	if (t.type != NUMBER) {      \
		if (t.type == ERROR) \
			return 1;    \
		prev_token = t;      \
		return 0;            \
	}
int
load_scene(FILE *in, float aspect_ratio)
{
	int bg_done;
	material *cur_material;

	tok = cur = lim = buf;
	eof = 0;
	line = 1;

	input = in;

	prev_token.type = ERROR;

	bg_done = 0;
	scene.cur_shape = 0;

	cur_material = &default_material;

loop:
	switch ((t = next_token()).type) {
	case KEYWORD:
		switch (t.k) {
		case CAMERA:
			PARSE(camera, &scene.camera, aspect_ratio);
			break;
		case BACKGROUND:
			if (bg_done) {
				fprintf(stderr,
				    "%s: second background definition on line %zu\n",
				    prog_name, line);
				return 1;
			}
			PARSE(texture, &scene.background);
			bg_done = 1;
			break;
		case MATERIAL:
			cur_material->next = malloc(sizeof(material));
			if (!cur_material->next) {
				fprintf(stderr,
				    "%s: memory allocation failed\n",
				    prog_name);
				return 1;
			}
			cur_material = cur_material->next;
			PARSE(material, cur_material);
			break;
		default:
			goto fail;
		}
		break;
	case SHAPE_TYPE:
		scene.shapes[scene.cur_shape].type = t.s;
		scene.shapes[scene.cur_shape].material = cur_material;
		switch (t.s) {
		case PLANE:
			PARSE(vec, scene.shapes[scene.cur_shape].p.normal);
			glm_vec4_normalize(
			    scene.shapes[scene.cur_shape].p.normal);
			scene.shapes[scene.cur_shape].p.d = CONSUME_FLOAT();
			glm_vec4_scale(scene.shapes[scene.cur_shape].p.normal,
			    scene.shapes[scene.cur_shape].p.d,
			    scene.shapes[scene.cur_shape].p.center);
			break;
		case SPHERE:
			PARSE(vec, scene.shapes[scene.cur_shape].s.center);
			scene.shapes[scene.cur_shape].s.r = CONSUME_FLOAT();
			break;
		}
		scene.cur_shape++;
		break;
	default:
	fail:
		*cur = 0;
		fprintf(stderr,
		    "%s: on line %zu expected a material, camera, shape or"
		    " background definition, got '%s'\n",
		    prog_name, line, tok);
	case ERROR:
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

	vh = fabsf(atanf(fov / 2.0)) * glm_vec4_norm(look) * 2.0;
	vw = vh * aspect_ratio;

	glm_vec3_proj(up, look, out->down);
	glm_vec4_sub(out->down, up, out->down);
	glm_vec4_scale_as(out->down, vh, out->down);

	glm_vec3_cross(look, out->down, out->right);
	glm_vec4_scale_as(out->right, vw, out->right);

	glm_vec4_copy(look_at, out->upper_left);
	glm_vec4_mulsubs(out->down, 0.5, out->upper_left);
	glm_vec4_mulsubs(out->right, 0.5, out->upper_left);

	return 0;
}

static int
parse_material(material *out)
{
	CONSUME(MATERIAL_TYPE);
	out->type = t.m;
	PARSE(texture, &out->texture);
	return 0;
}

static int
parse_texture(texture *out)
{
	switch ((t = next_token()).type) {
	case KEYWORD:
		if (t.k == BLACKBODY) {
			goto solid;
		} else if (t.k != KW_CHECKS)
			goto fail;
		out->type = CHECKS;
		TODO();
		break;
	case NUMBER:
	solid:
		prev_token = t;
		out->type = SOLID;
		PARSE(color, &out->solid);
		break;
	case STRING:
		out->type = IMAGE;
		out->image.data = stbi_loadf(t.str, &out->image.width,
		    &out->image.height, &out->image.n_channels, 0);
		if (out->image.data == NULL) {
			fprintf(stderr,
			    "%s: image file '%s' is corrupt or missing\n",
			    prog_name, t.str);
			return 1;
		}
		break;
	default:
	fail:
		*cur = 0;
		fprintf(stderr,
		    "%s: expected a texture on line %zu, got '%s'\n", prog_name,
		    line, tok);
	case ERROR:
		return 1;
	}
	return 0;
}

static int
parse_color(color *out)
{
	t = next_token();
	switch (t.type) {
	case KEYWORD:
		if (t.k != BLACKBODY)
			goto fail;
		TODO();
		break;
	case NUMBER:
		out->r = t.f;
		out->g = CONSUME_FLOAT();
		out->b = CONSUME_FLOAT();
		break;
	default:
	fail:
		*cur = 0;
		fprintf(stderr, "%s: expected a color on line %zu, got '%s'\n",
		    prog_name, line, tok);
	case ERROR:
		return 1;
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

int
hit_sphere(const sphere *sphere, const ray *ray, hit_info *out)
{
	vec oc;
	float a, half_b, c, discriminant;

	glm_vec4_sub((float *)ray->origin, (float *)sphere->center, oc);
	a = glm_vec4_norm2((float *)ray->d);
	half_b = glm_vec4_dot(oc, (float *)ray->d);
	c = glm_vec4_norm2(oc) - sphere->r * sphere->r;
	discriminant = half_b * half_b - a * c;

	out->t = (-half_b - sqrtf(discriminant)) / a;
	glm_vec4_copy((float *)ray->origin, out->p);
	glm_vec4_muladds((float *)ray->d, out->t, out->p);
	glm_vec4_sub(out->p, (float *)sphere->center, out->normal);
	glm_vec4_normalize(out->normal);

	return discriminant > 0.0;
}

int
hit_plane(const plane *plane, const ray *ray, hit_info *out)
{
	vec oc;
	float d;

	d = glm_vec4_dot((float *)plane->normal, (float *)ray->d);

	if (fabsf(d) > epsilon) {
		glm_vec4_sub((float *)plane->center, (float *)ray->origin, oc);
		out->t = glm_vec4_dot(oc, (float *)plane->normal) / d;
		glm_vec4_copy((float *)plane->normal, out->normal);
		glm_vec4_copy((float *)ray->origin, out->p);
		glm_vec4_muladds((float *)ray->d, out->t, out->p);
		return out->t >= 0;
	}

	return 0;
}

int
hit_scene(const ray *ray, hit_info *out)
{
	size_t i;
	int res;
	hit_info cur;

	out->t = INFINITY;

	for (i = 0; i < scene.cur_shape; i++) {
		switch (scene.shapes[i].type) {
		case SPHERE:
			res = hit_sphere(&scene.shapes[i].s, ray, &cur);
			break;
		case PLANE:
			res = hit_plane(&scene.shapes[i].p, ray, &cur);
			break;
		}
		if (res && cur.t < out->t && cur.t > epsilon) {
			*out = cur;
			out->material = scene.shapes[i].material;
		}
	}

	out->u = 0.0;
	out->v = 0.0;

	return out->t != INFINITY;
}
