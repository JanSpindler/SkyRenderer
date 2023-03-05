#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_debug_printf : enable

// info on renderpass used in application.
#include "renderpass.h"
#include "post_process.h"

layout (input_attachment_index = COLOR_ATTACHMENT_INDEX, set = PP_SETS_INPUT_ATTACHMENT, binding = 0) uniform subpassInput colorInput;

layout(location = 0) out vec4 out_color;

void main() {
	out_color = subpassLoad(colorInput);
}
