#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec2 frag_uv;

vec2 pos[4] = vec2[](
	vec2(-1.0, -1.0),
	vec2(-1.0, 1.0),
	vec2(1.0, 1.0),
	vec2(1.0, -1.0)
);

vec2 uv[4] = vec2[] (
	vec2(0.0, 0.0),
	vec2(0.0, 1.0),
	vec2(1.0, 1.0),
	vec2(1.0, 0.0)
);

int indices[6] = int[] ( 0, 1, 2, 2, 3, 0 );

void main()
{
	int index = indices[gl_VertexIndex];

	gl_Position = vec4(pos[index], 0.0, 1.0);
	frag_uv = uv[index];
}
