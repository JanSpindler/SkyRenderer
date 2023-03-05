#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "cam_set.h"

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;

layout (set = 0, binding = 0) uniform model_uniforms_t
{
	mat4 model_mat;
} model_ubo;

CAM_SET(1)

layout(location = 0) out vec2 frag_uv;
layout(location = 1) out vec3 frag_normal;

void main()
{
	gl_Position = cam.proj_view_mat * model_ubo.model_mat * vec4(pos, 1.0);
	frag_uv = uv;
	frag_normal = normal;
}
