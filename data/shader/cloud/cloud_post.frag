#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 frag_uv;

layout(set = 0, binding = 0) uniform screen_size_t
{
	int width;
	int height;
} screen_size;

layout(set = 0, binding = 1) uniform sampler2D cloud_tex;

layout(location = 0) out vec4 out_color;

#define GAUSS_KERNEL_SIZE 5

float[GAUSS_KERNEL_SIZE][GAUSS_KERNEL_SIZE] gauss_kernel = {
	{ 0.003765, 0.015019, 0.023792, 0.015019, 0.003765, },
	{ 0.015019, 0.059912, 0.094907, 0.059912, 0.015019 },
	{ 0.023792, 0.094907, 0.150342, 0.094907, 0.023792 },
	{ 0.015019, 0.059912, 0.094907, 0.059912, 0.015019 },
	{ 0.003765, 0.015019, 0.023792, 0.015019, 0.003765 } };

void main()
{
	vec4 color_sum = vec4(0.0);
	for (int x = 0; x < GAUSS_KERNEL_SIZE; x++)
	{
		for (int y = 0; y < GAUSS_KERNEL_SIZE; y++)
		{
			vec2 sample_offset = (vec2(x, y) - vec2(GAUSS_KERNEL_SIZE / 2)) / vec2(screen_size.width, screen_size.height);
			color_sum += texture(cloud_tex, frag_uv + sample_offset) * gauss_kernel[x][y];
		}
	}
	out_color = color_sum;

	//out_color = texture(cloud_tex, frag_uv);
}
