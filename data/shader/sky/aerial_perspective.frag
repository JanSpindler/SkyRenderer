#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_debug_printf : enable

#include "ap_renderer.h"
#include "scattering.h"
#include "aerial_perspective.h"
#include "cam_set.h"
#include "env_set.h"
#include "sun_set.h"
#include "functions.glsl"
#include "renderpass.h"

layout(location = 0) in vec3 pos_cam_relative;

SUN_SET(APR_SETS_SUN)

CAM_SET(APR_SETS_CAM)

layout (set = APR_SETS_RATIO, binding = 0) uniform ration_uniform_t {
	float ratio;
} ratio;

ENV_SET(APR_SETS_ENV0, env0)
ENV_SET(APR_SETS_ENV1, env1)

layout (input_attachment_index = DEPTH_ATTACHMENT_INDEX, set = APR_SETS_DEPTH_INPUT_ATTACHMENT, binding = 0) uniform subpassInput depthInput;
layout (input_attachment_index = COLOR_ATTACHMENT_INDEX, set = APR_SETS_COLOR_INPUT_ATTACHMENT, binding = 0) uniform subpassInput colorInput;

layout (set = APR_SETS_AERIAL_PERSPECTIVE, binding = APR_SETS_AERIAL_TRANSMITTANCE_BINDING) uniform sampler3D transmittance_image;
layout (set = APR_SETS_AERIAL_PERSPECTIVE, binding = APR_SETS_AERIAL_SCATTERING_BINDING) uniform sampler3D scattering_image;

layout(location = 0) out vec4 out_color;

void main()
{
	vec3 view_dir = normalize(pos_cam_relative);
	vec3 scoeff_r = mix(env0.rayleigh_scattering_coefficient, env1.rayleigh_scattering_coefficient, ratio.ratio);
	vec3 scoeff_m = mix(env0.mie_scattering_coefficient, env1.mie_scattering_coefficient, ratio.ratio);
	float asymmetry_factor = mix(env0.asymmetry_factor, env1.asymmetry_factor, ratio.ratio);

	vec3 object_color = subpassLoad(colorInput).rgb;

	// 0.5 -> 0, (max-1).5 -> 1.
	// TODO: maybe make sure width+height don't fall below two.
	float x = (gl_FragCoord.x - 0.5) / (cam.width-1);
	float y = (gl_FragCoord.y - 0.5) / (cam.height-1);
	float z = linearize_depth(subpassLoad(depthInput).r, cam.near, cam.far);

	// 1-y for flipped viewport (I think).
	vec3 tex_coord = tex_address_shifted(vec3(x,1-y,z), vec3(AP_X, AP_Y, AP_Z));

	vec3 transmittance = texture(transmittance_image, tex_coord).xyz;
	vec4 scattering_r_rgb_m_r = texture(scattering_image, tex_coord);

	// use mixed scattering coefficients, the textures were already created
	// with them, little sense to un-mix now.
	//
	// TODO (maybe): create two textures for aerial perspective (downside
	// performance, 2x as much work for AP each frame).
	vec3 insc_m_rgb = mie_from_rayleigh(scattering_r_rgb_m_r, scoeff_r, scoeff_m);

	// final color is scattering from sun + transmittance from object.
	vec3 inscattering_sun = sun.color * (phase_m(dot(view_dir, sun.sun_dir), asymmetry_factor)*insc_m_rgb +
										   phase_r(dot(view_dir, sun.sun_dir))*scattering_r_rgb_m_r.rgb);
	vec3 attenuated_object = transmittance * object_color;

	// inscattering_sun becomes nan if inscattering is 0.
	out_color = vec4(!isnan(inscattering_sun.x) ? attenuated_object+inscattering_sun : attenuated_object, 1);
}
