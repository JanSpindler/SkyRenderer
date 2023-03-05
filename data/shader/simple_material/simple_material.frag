#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 frag_uv;
layout(location = 1) in vec3 frag_normal;

layout (set = 2, binding = 0) uniform material_uniform_t
{
	vec4 diffuse_color;
	uint use_diffuse_tex;
} material_ubo;

layout (set = 2, binding = 1) uniform sampler2D material_diffuse_tex;

layout(set = 3, binding = 0) uniform sun_t
{
	vec3 color;
	float zenith;
	vec3 dir;
	float azimuth;
} sun;

layout(set = 4, binding = 0) uniform samplerCube environment_diffuse;

layout (location = 0) out vec4 out_color;

void main()
{
	vec3 normal = normalize(frag_normal);

	if (material_ubo.use_diffuse_tex == 1)
		out_color = texture(material_diffuse_tex, frag_uv);
	else
		out_color = material_ubo.diffuse_color;

	out_color.rgb *= texture(environment_diffuse, normal).rgb;
}
