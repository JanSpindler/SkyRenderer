#version 450
#extension GL_ARB_separate_shader_objects : enable
// #extension GL_ARG_shader_explicit_arithmetic_types_float64: enable

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
}
