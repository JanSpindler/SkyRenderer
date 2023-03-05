#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_debug_printf : enable

// info on renderpass used in application.
#include "renderpass.h"
#include "post_process.h"

layout (input_attachment_index = COLOR_ATTACHMENT_INDEX, set = PP_SETS_INPUT_ATTACHMENT, binding = 0) uniform subpassInput colorInput;

layout(location = 0) out vec4 out_color;

float exposure = 0.5;

void main() {
	vec3 in_color = subpassLoad(colorInput).xyz;
	/* in_color = vec3(1) - exp(-in_color*exposure); */
	in_color = pow(in_color, vec3(1/2.2f));
	out_color = vec4(in_color, 1);
}
