#version 450
#extension GL_ARB_separate_shader_objects : enable
// #extension GL_ARG_shader_explicit_arithmetic_types_float64: enable

#include "sky.h"
#include "cam_set.h"

CAM_SET(SKY_SETS_CAM)

layout (location = 0) out vec3 pos_cam_relative;

vec2 pos[4] = vec2[](
	vec2(-1.0, -1.0),
	vec2(-1.0, 1.0),
	vec2(1.0, 1.0),
	vec2(1.0, -1.0)
);

int indices[6] = int[] (0,2,1,0,3,2);

void main()
{
	int index = indices[gl_VertexIndex];

	gl_Position = vec4(pos[index], 1.0, 1.0);
	dvec4 pos = dmat4(cam.proj_view_mat_inv)*dvec4(gl_Position);
	// normalize
	pos_cam_relative = vec3(pos.xyz/pos.w - dvec3(cam.pos));
	// vec4 pos = inverse(camera_ubo.proj_view_mat)*gl_Position;
	// // normalize
	// world_pos = pos.xyz/pos.w;
}
