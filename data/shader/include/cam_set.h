#define CAM_SET(SET_INDEX) layout (set = SET_INDEX, binding = 0) uniform camera_uniforms_t \
{                                                                                          \
	dmat4 proj_view_mat_inv;                                                               \
	mat4 proj_view_mat;                                                                    \
	vec3 pos;                                                                              \
	float near;                                                                            \
	float far;                                                                             \
	float width;                                                                           \
	float height;                                                                          \
} cam;

