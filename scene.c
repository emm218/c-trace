#include <ctype.h>
#include <errno.h>
#include <stb_image.h>
#include <stdlib.h>
#include <string.h>

#include "scene.h"
#include "token.h"

static void compute_bg_cdf(void);

static int parse_camera(camera *, float);
static int parse_color(color *);
static int parse_material(material *);
static int parse_plane(plane *);
static int parse_texture(texture *);
static int parse_vec(vec);

#define TODO()                      \
	fprintf(stderr, "todo!\n"); \
	return 1;

#define BUF_SIZE 4096

static material default_material = {
	.type = DIFFUSE,
	.texture = {
		.type = SOLID,
		.solid = { 0.5, 0.5, 0.5 },
	},
};

static vec vec_i = { 1.0, 0.0, 0.0, 0.0 };
static vec vec_j = { 0.0, 1.0, 0.0, 0.0 };
// static vec vec_k = { 0.0, 0.0, 1.0, 0.0 };

static const char *token_types[] = {
	"'('",
	"')'",
	"KEYWORD",
	"a shape type",
	"a material type",
	"a number",
	"a string",
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
	scene.materials = &default_material;
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
			PARSE(texture, &scene.bg.tex);
			compute_bg_cdf();
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
			cur_material->next = NULL;
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
			PARSE(plane, &scene.shapes[scene.cur_shape].p);
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
		out->checks.scale = CONSUME_FLOAT();
		PARSE(color, &out->checks.c1);
		PARSE(color, &out->checks.c2);
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
parse_plane(plane *out)
{

	PARSE(vec, out->normal);
	glm_vec4_normalize(out->normal);
	out->d = CONSUME_FLOAT();
	glm_vec4_scale(out->normal, out->d, out->center);
	glm_vec3_cross(out->normal, vec_i, out->u);
	if (glm_vec4_norm2(out->u) < epsilon) {
		glm_vec3_cross(out->normal, vec_j, out->u);
	}
	glm_vec4_normalize(out->u);
	glm_vec3_cross(out->normal, out->u, out->v);

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

static void
compute_bg_cdf()
{
	size_t x, y, width, height;
	float u, v, total, row_total, sample;

	switch (scene.bg.tex.type) {
	case CHECKS:
		width = scene.bg.tex.checks.scale;
		height = scene.bg.tex.checks.scale;
		break;
	case SOLID:
		width = 1;
		height = 1;
		break;
	case IMAGE:
		width = scene.bg.tex.image.width;
		height = scene.bg.tex.image.height;
		break;
	}

	scene.bg.w = width;
	scene.bg.h = height;

	scene.bg.pdf = malloc(sizeof(float) * width * height);
	scene.bg.cdf_c = malloc(sizeof(float) * width * height);
	scene.bg.cdf_m = malloc(sizeof(float) * width);
	if (!scene.bg.cdf_c || !scene.bg.cdf_m || !scene.bg.pdf) {
		fprintf(stderr, "%s: malloc failed\n", prog_name);
		exit(1);
	}

	total = 0.0;

	for (y = 0; y < height; y++) {
		v = ((float)y + 0.5) / height;
		row_total = 0.0;
		for (x = 0; x < width; x++) {
			u = ((float)x + 0.5) / width;
			sample = sample_intensity(&scene.bg.tex, u, v) *
			    sinf(GLM_PI * v);
			scene.bg.pdf[y * width + x] = sample;
			row_total += sample;
			scene.bg.cdf_c[y * width + x] = row_total;
		}
		for (x = 0; x < width; x++) {
			scene.bg.cdf_c[y * width + x] /= row_total;
		}
		total += row_total;
		scene.bg.cdf_m[y] = total;
	}
	for (y = 0; y < height; y++) {
		scene.bg.cdf_m[y] /= total;
	}
	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			scene.bg.pdf[y * width + x] /= total;
		}
	}
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

	return out->t != INFINITY;
}
