#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_debug_printf : enable

#include "sky.h"
#include "scattering.h"
#include "aerial_perspective.h"
#include "cam_set.h"
#include "env_set.h"
#include "sun_set.h"
#include "functions.glsl"
#include "renderpass.h"

layout(location = 0) in vec3 pos_cam_relative;

SUN_SET(SKY_SETS_SUN)

CAM_SET(SKY_SETS_CAM)

layout (set = SKY_SETS_SCTEXTURE0, binding = 0) uniform sampler3D tex0;
layout (set = SKY_SETS_SCTEXTURE1, binding = 0) uniform sampler3D tex1;
layout (set = SKY_SETS_RATIO, binding = 0) uniform ration_uniform_t {
	float ratio;
} ratio;

ENV_SET(SKY_SETS_ENV0, env0)
ENV_SET(SKY_SETS_ENV1, env1)

layout(location = 0) out vec4 out_color;

// only use if not inside atmosphere.
// o+ret*u is intersection-point.
// https://en.wikipedia.org/wiki/Line%E2%80%93sphere_intersection
double atmosphere_intersect(dvec3 o, dvec3 u, dvec3 c, double r) {
	dvec3 sphere_relative = o - c;
	double a = dot(u, sphere_relative);
	double center_dist = length(sphere_relative);
	double under_root = a*a - center_dist*center_dist + r*r;
	if (under_root < 0)
		// no intersection.
		return INFINITY;
	else {
		double res = -a-sqrt(under_root);
		if (res < 0) return INFINITY;
		return res;
	}
}

void main()
{
	vec3 view_dir = normalize(pos_cam_relative);
	float r_planet = mix(env0.r_planet, env1.r_planet, ratio.ratio);
	float atmosphere_height = mix(env0.atmosphere_height, env1.atmosphere_height, ratio.ratio);
	float r_atmosphere = mix(env0.r_atmosphere, env1.r_atmosphere, ratio.ratio);

	vec3 earth_center = vec3(0, -r_planet, 0);

	// cam.pos += vec3(0, 6000000, 0);

	vec3 up;
	float height;

	if (length(cam.pos - earth_center) > r_atmosphere) {
		// cam is not inside atmosphere.
		double atm_intersect_t = atmosphere_intersect(dvec3(cam.pos), dvec3(view_dir), dvec3(earth_center), r_atmosphere);
		if (atm_intersect_t == INFINITY) {
			// view has no intersection with atmosphere, return space-black for now.
			out_color = vec4(0,0,0,1);
			return;
		}

		up = normalize(vec3(dvec3(cam.pos) + atm_intersect_t*dvec3(view_dir)-dvec3(earth_center)));
		height = atmosphere_height;
	} else {
		up = normalize(cam.pos-earth_center);
		height = length(cam.pos-earth_center) - r_planet;
	}

	// cosine of view angle, assume that world-"up" is positive y.
	float view_angle_c = dot(up, view_dir);
	float sun_angle_c = dot(up, sun.sun_dir);

	float tex_x = height_to_tex(height, atmosphere_height);
	float tex_y = view_to_tex(view_angle_c, height, r_planet);
	float tex_z = sun_to_tex(sun_angle_c);

	// imageStore writes to centers of texel, reading a texel with any component 0 or 1 interpolates with border (or edge) color.
	// Shift coordinates so that the borders of the sampled range go through the centers of the texels.
	vec3 tex_coord = tex_address_shifted(vec3(tex_x, tex_y, tex_z), vec3(
		SCATTERING_RESOLUTION_HEIGHT,
		SCATTERING_RESOLUTION_VIEW,
		SCATTERING_RESOLUTION_SUN));

	vec4 r_rgb_m_r0 = texture(tex0, tex_coord);
	vec3 m_rgb0 = mie_from_rayleigh(r_rgb_m_r0, env0.rayleigh_scattering_coefficient, env0.mie_scattering_coefficient);
	vec3 color0 = sun.color * (phase_m(dot(view_dir, sun.sun_dir), env0.asymmetry_factor)*m_rgb0 +
                                  phase_r(dot(view_dir, sun.sun_dir))*r_rgb_m_r0.rgb);
	// can happen if texture contained 0.
	if (isnan(color0.x))
		color0 = vec3(0,0,0);

	vec4 r_rgb_m_r1 = texture(tex1, tex_coord);
	vec3 m_rgb1 = mie_from_rayleigh(r_rgb_m_r1, env1.rayleigh_scattering_coefficient, env1.mie_scattering_coefficient);
	vec3 color1 = sun.color * (phase_m(dot(view_dir, sun.sun_dir), env1.asymmetry_factor)*m_rgb1 +
                                  phase_r(dot(view_dir, sun.sun_dir))*r_rgb_m_r1.rgb);
	if (isnan(color1.x))
		color1 = vec3(0,0,0);

	out_color = vec4(mix(color0, color1, ratio.ratio), 1);

	// if (gl_FragCoord.xy == vec2(0.5, 0.5)) {
	// 	debugPrintfEXT("%f", ratio.ratio);
	// 	debugPrintfEXT("%v4f", out_color);
	// 	debugPrintfEXT("%v3f :: %v3f", color0, color1);
	// }

	// if (view_angle_c < 0 && out_color.b > .2) {
	// 	debugPrintfEXT("%f : %f : %v4f : %v4f", view_angle_c, tex_to_view(tex_y, cam.pos.y),
	// 		r_rgb_m_r, single_scattering(cam.pos.y, unit_vec_from_cos(view_angle_c), -unit_vec_from_cos(sun_angle_c)));
	// }

	// out_color = vec4(texture(tex, vec3(
	// 	0.5*(1.0f/SCATTERING_RESOLUTION_HEIGHT)+(1/(SCATTERING_RESOLUTION_HEIGHT-1))*gl_FragCoord.x/800.0f,
	// 	0.5*(1.0f/SCATTERING_RESOLUTION_VIEW)+(1/(SCATTERING_RESOLUTION_VIEW-1))*gl_FragCoord.y/600.0f,
	// 	.5)));
}
