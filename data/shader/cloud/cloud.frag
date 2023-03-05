#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_debug_printf : enable

#include "scattering.h"
#include "transmittance.h"
#include "functions.glsl"

layout(location = 0) in vec3 pixel_world_pos;
layout(location = 1) in vec2 frag_uv;

layout(constant_id = 0) const int SAMPLE_COUNT = 16;
layout(constant_id = 1) const int SECONDARY_SAMPLE_COUNT = 0;

#include "cam_set.h"
CAM_SET(0)

layout(set = 1, binding = 0) uniform sun_t
{
	vec3 color;
	float zenith;
	vec3 dir;
	float azimuth;
} sun;

layout(set = 2, binding = 0) uniform cloud_data_t
{
	vec3 sky_size;
	vec3 sky_pos;
	float shape_scale;
	float detail_threshold;
	float detail_factor;
	float detail_scale;
	float height_gradient_min_val;
	float height_gradient_max_val;
	float g;
	float ambient_gradient_min_val;
	float ambient_gradient_max_val;
	float jitter_strength;
	float sigma_s;
	float sigma_e;
} cloud_data;

layout(set = 2, binding = 1) uniform sampler3D cloud_shape_tex;
layout(set = 2, binding = 2) uniform sampler3D cloud_detail_tex;
layout(set = 2, binding = 3) uniform sampler2D weather_tex;

layout(input_attachment_index = 0, set = 3, binding = 0) uniform subpassInput geometry_depth;

layout(set = 4, binding = 0) uniform wind_t
{
	vec2 offset;
} wind;

layout (set = 5, binding = 0) uniform sampler3D scattering0;
layout (set = 6, binding = 0) uniform sampler3D scattering1;
layout (set = 7, binding = 0) uniform sampler2D transmittance0;
layout (set = 8, binding = 0) uniform sampler2D transmittance1;

#include "env_set.h"
ENV_SET(9, env0)
ENV_SET(10, env1)

layout (set = 11, binding = 0) uniform ration_uniform_t {
	float ratio;
} ratio;


layout(location = 0) out vec4 out_color;

#define PI 3.14159265359

#define MAX_SECONDARY_SAMPLE_COUNT 8
#define MAX_RAY_DISTANCE 100000.0
#define MIN_RAY_DISTANCE 0.125

#define MIN_HEIGHT (cloud_data.sky_pos.y - (cloud_data.sky_size.y / 2))
#define MAX_HEIGHT (MIN_HEIGHT + cloud_data.sky_size.y)

struct cloud_result_t
{
	vec3 light;
	float transmittance;
};

// set in main.
float r_planet;
float atmosphere_height;

vec3 to_sky_model_vec(vec3 world_vec) {
	// shift coordinate system r_planet units down
	// <=> shift vector r_planet units up.
	return world_vec + vec3(0, r_planet, 0);
}

// o+ret*u is intersection-point.
// https://en.wikipedia.org/wiki/Line%E2%80%93sphere_intersection
float sphere_intersect(vec3 o, vec3 u, vec3 c, float r) {
	vec3 sphere_relative = o - c;
	float a = dot(u, sphere_relative);
	float center_dist = length(sphere_relative);
	float under_root = a*a - center_dist*center_dist + r*r;
	if (under_root < 0)
		// no intersection.
		return INFINITY;
	else {
		float res = -a-sqrt(under_root);
		if (res < 0) return INFINITY;
		return res;
	}
}

vec3 mixed_transmittance(vec3 pos) {
	pos = to_sky_model_vec(pos);
	float height = height(pos, r_planet);
	float view_cos = dot(normalize(pos), sun.dir);

	bool earth_intersected = sphere_intersect(pos, sun.dir, vec3(0,0,0), r_planet) == INFINITY ? false : true;

	return earth_intersected ? vec3(0,0,0) : mix(
		_fetch_transmittance(
			height,
			view_cos,
			r_planet,
			atmosphere_height,
			vec2(TRANSMITTANCE_RESOLUTION_HEIGHT, TRANSMITTANCE_RESOLUTION_VIEW),
			transmittance0),
		_fetch_transmittance(
			height,
			view_cos,
			r_planet,
			atmosphere_height,
			vec2(TRANSMITTANCE_RESOLUTION_HEIGHT, TRANSMITTANCE_RESOLUTION_VIEW),
			transmittance1),
		ratio.ratio);
}

vec3 ambient(vec3 p, vec3 view) {
	p = to_sky_model_vec(p);

	float height = height(p, r_planet);
	float tex_x = height_to_tex(height, atmosphere_height);
	float tex_y = view_to_tex(dot(p, view), height, r_planet);
	float tex_z = sun_to_tex(dot(normalize(p), sun.dir));

	vec3 tex_coord = tex_address_shifted(vec3(tex_x, tex_y, tex_z), vec3(
		SCATTERING_RESOLUTION_HEIGHT,
		SCATTERING_RESOLUTION_VIEW,
		SCATTERING_RESOLUTION_SUN));

	vec4 r_rgb_m_r0 = texture(scattering0, tex_coord);
	vec3 m_rgb0 = mie_from_rayleigh(r_rgb_m_r0, env0.rayleigh_scattering_coefficient, env0.mie_scattering_coefficient);
	vec3 color0 = sun.color * (
		phase_m(dot(view, sun.dir), env0.asymmetry_factor)*m_rgb0 +
		phase_r(dot(view, sun.dir))*r_rgb_m_r0.rgb);

	vec4 r_rgb_m_r1 = texture(scattering1, tex_coord);
	vec3 m_rgb1 = mie_from_rayleigh(r_rgb_m_r1, env1.rayleigh_scattering_coefficient, env1.mie_scattering_coefficient);
	vec3 color1 = sun.color * (
		phase_m(dot(view, sun.dir), env1.asymmetry_factor)*m_rgb1 +
		phase_r(dot(view, sun.dir))*r_rgb_m_r1.rgb);

	// debugPrintfEXT("%v3f", mix(color0, color1, ratio.ratio));

	return mix(color0, color1, ratio.ratio);
}

float rand(vec2 co)
{
	return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

float sky_sdf(vec3 pos)
{
	vec3 sky_size = cloud_data.sky_size;// * vec3(4.0, 1.0, 4.0); // TODO: make ubo parameter
	vec3 d = abs(pos - cloud_data.sky_pos) - sky_size / 2;
	return length(max(d, 0)) + min(max(d.x, max(d.y, d.z)), 0);
}

vec3[2] find_entry_exit(vec3 ro, vec3 rd)
{
	// rd should be normalized

	float dist;
	do
	{
		dist = sky_sdf(ro);
		ro  += dist * rd;
	} while (dist > MIN_RAY_DISTANCE && dist < MAX_RAY_DISTANCE);
	vec3 entry = ro;

	ro += rd * length(2 * cloud_data.sky_size);//ro += rd * MAX_RAY_DISTANCE * 2;
	rd *= -1.0;
	do
	{
		dist = sky_sdf(ro);
		ro += dist * rd;
	} while (dist > MIN_RAY_DISTANCE && dist < MAX_RAY_DISTANCE);
	vec3 exit = ro;
	
	return vec3[2]( entry, exit );
}

void gen_sample_points(vec3 start_pos, vec3 end_pos, out vec3 samples[SAMPLE_COUNT])
{
	vec3 dir = end_pos - start_pos;
	for (int i = 0; i < SAMPLE_COUNT; i++)
			samples[i] = start_pos + dir * (float(i) / float(SAMPLE_COUNT));
}

vec3 get_sky_uvw(vec3 pos)
{
	return ((pos - cloud_data.sky_pos) / cloud_data.sky_size) + vec3(0.5);
}

float get_cloud_shape(vec3 pos)
{
	vec3 tex_sample_pos = get_sky_uvw(pos);

	vec4 shape = texture(cloud_shape_tex, tex_sample_pos * cloud_data.shape_scale);
	float result = shape.x * (shape.y + shape.z + shape.w);
	return result;
}

float get_cloud_detail(vec3 pos)
{
	vec3 tex_sample_pos = get_sky_uvw(pos);

	vec3 detail = texture(cloud_shape_tex, tex_sample_pos * cloud_data.detail_scale).xyz;
	float result = detail.x + detail.y + detail.z;
	result /= 3.0;
	return result;
}

vec3 get_weather(vec3 pos)
{
	return texture(weather_tex, get_sky_uvw(pos).xz + fract(wind.offset)).xyz;
}

float get_height_signal(float height, float altitude, vec3 pos)
{
	float real_altitude = pos.y;
	
	float r1 = real_altitude - altitude;
	float r2 = real_altitude - altitude - height;
	float s = -4.0 / (height * height);

	float result = r1 * r2 * s;

	return result;
}

float get_height_gradient(float height, float altitude, vec3 pos)
{
	float real_altitude = pos.y;
	
	float x = (real_altitude - altitude) / height;
	x = clamp(x, 0.0, 1.0);
	float result = cloud_data.height_gradient_min_val + (cloud_data.height_gradient_max_val - cloud_data.height_gradient_min_val) * x;

	return result;
}

// Henyey-Greenstein
float hg_phase_func(float cos_theta, float g)
{
	float g2 = g * g;
	float result = 0.5 * (1 - g2) / pow(1 + g2 - (2 * g * cos_theta), 1.5);
	return result;
}

float get_ambient_gradient(const float height)
{
	const float x = (height - MIN_HEIGHT) / (MAX_HEIGHT - MIN_HEIGHT);
	return cloud_data.ambient_gradient_min_val + x * (cloud_data.ambient_gradient_max_val - cloud_data.ambient_gradient_min_val);
}

float get_density(vec3 pos)
{
	vec3 weather = get_weather(pos);
	float density = weather.r;
	float height = weather.g * cloud_data.sky_size.y;
	float altitude = MIN_HEIGHT + weather.b * cloud_data.sky_size.y;

	density *= get_height_signal(height, altitude, pos);
	density *= get_cloud_shape(pos);
	if (density < cloud_data.detail_threshold)
		density -= get_cloud_detail(pos) * cloud_data.detail_factor;
	density *= get_height_gradient(height, altitude, pos);

	return clamp(density, 0.0, 1.0);
}

float get_self_shadowing(vec3 pos)
{
	// Exit if not used
	if (SECONDARY_SAMPLE_COUNT == 0)
		return 1.0;

	// Find exit from current pos
	const vec3 exit = find_entry_exit(pos, normalize(sun.dir))[1]; // inv dir

	// Generate secondary sample points using lerp factors
	const vec3 direction = exit - pos;
	vec3 secondary_sample_points[MAX_SECONDARY_SAMPLE_COUNT];
	for (int i = 0; i < SECONDARY_SAMPLE_COUNT; i++)
		secondary_sample_points[i] = pos + direction * exp(float(i - SECONDARY_SAMPLE_COUNT));

	// Calculate light reaching pos
	float transmittance = 1.0;
	const float sigma_e = cloud_data.sigma_e;
	for (int i = 0; i < SECONDARY_SAMPLE_COUNT; i++)
	{
		const vec3 sample_point = secondary_sample_points[i];
		const float density = get_density(sample_point);
		if (density > 0.0)
		{
			const float sample_sigma_e = sigma_e * density;
			const float step_size = (i < SECONDARY_SAMPLE_COUNT - 1) ? 
				length(secondary_sample_points[i + 1] - secondary_sample_points[i]) : 
				1.0;
			const float t_r = exp(-sample_sigma_e * step_size);
			transmittance *= t_r;
		}
	}

	return transmittance;
}

cloud_result_t render_cloud(vec3 sample_points[SAMPLE_COUNT], vec3 out_dir, vec3 ro)
{
	// out_dir should be normalized
	
	cloud_result_t result;
	result.light = vec3(0.0);
	result.transmittance = 1.0;
	
	const float sigma_s = cloud_data.sigma_s;
	const float sigma_e = cloud_data.sigma_e;
	const float step_size = length(sample_points[1] - sample_points[0]);
	const vec3 step_offset = rand(vec2(out_dir.x * out_dir.y, out_dir.z)) * out_dir * step_size * cloud_data.jitter_strength;

	const float geom_depth = subpassLoad(geometry_depth).r;
	vec4 geom_screen_coord = vec4(frag_uv, geom_depth, 1.0);
	vec4 geom_world_pos4 = vec4(cam.proj_view_mat_inv * geom_screen_coord);
	vec3 geom_world_pos = geom_world_pos4.xyz / geom_world_pos4.w;

	for (int i = 0; i < SAMPLE_COUNT; i++)
	{
		const vec3 sample_point = sample_points[i] + step_offset;
		const float density = get_density(sample_point);
		
		if (distance(ro, geom_world_pos) < distance(ro, sample_point))
			break;

		if (density > 0.0)
		{
			const float sample_sigma_s = sigma_s * density;
			const float sample_sigma_e = sigma_e * density;

			const vec3 ambient = get_ambient_gradient(sample_point.y) * ambient(sample_point, -out_dir);

			// attenuate direct sunlight with transmittance.
			const vec3 phase_result = mixed_transmittance(sample_point)*hg_phase_func(dot(-sun.dir, out_dir), cloud_data.g); // inv dir
			const float self_shadowing = get_self_shadowing(sample_point);

			const vec3 s = (vec3(self_shadowing * phase_result) + ambient) * sample_sigma_s;
			const float t_r = exp(-sample_sigma_e * step_size);
			const vec3 s_int = (s - (s * t_r)) / sample_sigma_e;

			result.light += result.transmittance * s_int;
			result.transmittance *= t_r;

			// Early exit
			if (result.transmittance < 0.01)
				break;
		}
	}

	return result;
}

void main()
{
	r_planet = mix(env0.r_planet, env1.r_planet, ratio.ratio);
	atmosphere_height = mix(env0.atmosphere_height, env1.atmosphere_height, ratio.ratio);

	const vec3 ro = cam.pos;
	const vec3 rd = normalize(pixel_world_pos - ro);

	const vec3[2] entry_exit = find_entry_exit(ro, rd);
	const vec3 entry = entry_exit[0];
	const vec3 exit = entry_exit[1];

	if (sky_sdf(entry) > MAX_RAY_DISTANCE)
	{
		out_color = vec4(0.0);
		return;
	}

	vec3 sample_points[SAMPLE_COUNT];
	gen_sample_points(entry, exit, sample_points);
	cloud_result_t result = render_cloud(sample_points, -rd, ro);
	out_color = vec4(result.light, 1.0 - result.transmittance);

	gl_FragDepth = 0.0;
}
