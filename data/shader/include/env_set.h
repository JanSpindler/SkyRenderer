#define ENV_SET(SET_INDEX, NAME) layout (set = SET_INDEX, binding = 0) uniform NAME ## _uniform_t { \
	vec3 rayleigh_scattering_coefficient;                                             \
	float r_planet;                                                                   \
	vec3 mie_scattering_coefficient;                                                  \
	float r_atmosphere;                                                               \
	vec3 ozone_extinction_coefficient;                                                \
	float atmosphere_height;                                                          \
	float asymmetry_factor;                                                           \
	float rayleigh_scale_height;                                                      \
	float mie_scale_height;                                                           \
} NAME;
