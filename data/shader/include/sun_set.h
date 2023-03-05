#define SUN_SET(SET_INDEX) layout (set = SET_INDEX, binding = 0) uniform sun_uniforms_t \
{                                                                                       \
	vec3 color;                                                                         \
	float zenith;                                                                       \
	vec3 sun_dir;                                                                       \
	float azimuth;                                                                      \
} sun;

