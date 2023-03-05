#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "cam_set.h"

vec2 pos[4] = vec2[](
	vec2(-1.0, -1.0),
	vec2(-1.0, 1.0),
	vec2(1.0, 1.0),
	vec2(1.0, -1.0));

int indices[6] = int[] ( 0, 1, 2, 2, 3, 0 );

CAM_SET(0)

layout(location = 0) out vec3 pixel_world_pos;
layout(location = 1) out vec2 frag_uv;

void main()
{
	int index = indices[gl_VertexIndex];
	gl_Position = vec4(pos[index], 0.0, 1.0);

	vec4 screen_coord = gl_Position;
	screen_coord.y *= -1.0;

	vec4 world_pos = vec4(cam.proj_view_mat_inv * screen_coord);
	pixel_world_pos = world_pos.xyz / world_pos.w;
}
