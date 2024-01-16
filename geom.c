#include "geom.h"

const float epsilon = 0.0001f;

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
	vec oc, pc;
	float d;

	d = glm_vec4_dot((float *)plane->normal, (float *)ray->d);

	if (fabsf(d) > epsilon) {
		glm_vec4_sub((float *)plane->center, (float *)ray->origin, oc);
		out->t = glm_vec4_dot(oc, (float *)plane->normal) / d;
		glm_vec4_copy((float *)plane->normal, out->normal);
		glm_vec4_copy((float *)ray->origin, out->p);
		glm_vec4_muladds((float *)ray->d, out->t, out->p);

		glm_vec4_sub(out->p, (float *)plane->center, pc);

		out->u = glm_vec4_dot((float *)plane->u, pc);
		out->v = glm_vec4_dot((float *)plane->v, pc);

		out->u = out->u - floorf(out->u);
		out->v = out->v - floorf(out->v);

		return out->t >= 0;
	}

	return 0;
}
