// higher steps removes slight artefacts in blue.
const float INTEGRATION_STEPS=50;

// no predifined constant, but this works.
const float INFINITY = 1.0/0.0;

const float pi = 3.1415;

float phase_r(float diff_cos) {
	return (8.0f/10.0f)*((7.0f/5.0f)+diff_cos/2.0f);
}

// g is asymmetry factor.
float phase_m(float diff_cos, float g) {
	return (3*(1-g*g)*(1+diff_cos*diff_cos))/((4+2*g*g)*pow(1+g*g-2*g*diff_cos, 3.0f/2.0f));
}

float density_r(float height, float r_scale_height) {
	return exp(-height/r_scale_height);
}

float density_m(float height, float m_scale_height) {
	return exp(-height/m_scale_height);
}

float density_o(float height, float r_scale_height) {
	return density_r(height, r_scale_height)*6e-7;
}

float height(vec2 p, float rad_e) {
	return max(length(p)-rad_e, 0.3);
}

float height(vec3 p, float rad_e) {
	return length(p)-rad_e;
}

vec3 transmittance(vec2 p1, vec2 p2, float r_scale_height, float m_scale_height, vec3 extcoeff_r, vec3 extcoeff_m, vec3 extcoeff_o, float rad_e) {
	// TODO: integrate using constant step_size, variate number of steps instead?
	// also: try quadrature instead of trapezoidal integration (supposed to be better).

	// no attenuation for same position.
	if (length(p2-p1) < 1)
		return vec3(1,1,1);
	float step_length = length(p2-p1)/INTEGRATION_STEPS;
	vec2 dir = normalize(p2-p1);

	float dens_m_sum = 0;
	float dens_r_sum = 0;
	float dens_o_sum = 0;
	float dens_m_prev = density_m(height(p1, rad_e), m_scale_height);
	float dens_r_prev = density_r(height(p1, rad_e), r_scale_height);
	float dens_o_prev = density_o(height(p1, rad_e), r_scale_height);

	// first sample is p1, last sample is p2.
	for(int i=1; i <= INTEGRATION_STEPS; ++i) {
		vec2 s = p1 + step_length*i*dir;
		float dens_m = density_m(height(s, rad_e), m_scale_height);
		float dens_r = density_r(height(s, rad_e), r_scale_height);
		float dens_o = density_o(height(s, rad_e), r_scale_height);

		dens_m_sum += (dens_m_prev + dens_m) / 2.0f*step_length;
		dens_r_sum += (dens_r_prev + dens_r) / 2.0f*step_length;
		dens_o_sum += (dens_o_prev + dens_o) / 2.0f*step_length;

		dens_m_prev = dens_m;
		dens_r_prev = dens_r;
		dens_o_prev = dens_o;
	}

	return exp(-(dens_m_sum*extcoeff_m + dens_r_sum*extcoeff_r + dens_o_sum*extcoeff_o));
}

// @brief returns x such that |p+x*dir| == ih1, INFINITY on no intersection.
// @param dir: unit vector.
// @param h: radius of circle to intersect.
float concentric_circle_intersect_x(vec2 p, vec2 dir, float ih1) {
	float pdd = dot(p, dir);
	// under root.
	float x_part = pdd*pdd - dot(p, p) + ih1*ih1;
	// no intersection.
	if (x_part < 0)
		return INFINITY;
	x_part = sqrt(x_part);
	float res1 = -pdd - x_part;
	// find x for ray (not line!).
	if (res1 >= 0)
		return res1;
	else {
		res1 = -pdd + x_part;
		// no negative intersectionss.
		if (res1 >= 0)
			return res1;
		else
			return INFINITY;
	}
}

// @param h: height from earth-surface.
// @param dir: unit vector, direction away from eye.
// @return vec2(INFINITY, INFINITY) on no intersection.
vec2 atmosphere_earth_intersect(vec2 p, vec2 dir, float rad_e, float rad_a) {
	// if an intersection with the earth exists, it occurs before the ray
	// intersects the atmosphere (earth completely inside atmosphere).
	float ea_intsct = concentric_circle_intersect_x(p, dir, rad_e);
	if (ea_intsct == INFINITY)
		// may also be (INFINITY, INFINITY).
		return p + concentric_circle_intersect_x(p, dir, rad_a)*dir;
	else
		return p + ea_intsct*dir;
}

// shift samples so 0,0,0 and 1,1,1 are at the texel-centers, not on the border.
vec3 tex_address_shifted(vec3 texcoord_range_zero_one, vec3 res) {
	return vec3(
		.5f/res.x+((float(res.x-1))/res.x)*texcoord_range_zero_one.x,
		.5f/res.y+((float(res.y-1))/res.y)*texcoord_range_zero_one.y,
		.5f/res.z+((float(res.z-1))/res.z)*texcoord_range_zero_one.z
	);
}

// shift samples so 0,0 and 1,1 are at the texel-centers, not on the border.
vec2 tex_address_shifted(vec2 texcoord_range_zero_one, vec2 res) {
	return vec2(
		.5f/res.x+((float(res.x-1))/res.x)*texcoord_range_zero_one.x,
		.5f/res.y+((float(res.y-1))/res.y)*texcoord_range_zero_one.y
	);
}

// shift samples so 0,0 and 1,1 are at the texel-centers, not on the border.
float tex_address_shifted(float texcoord_range_zero_one, float res) {
	return .5f/res.x+((res.x-1)/res.x)*texcoord_range_zero_one.x;
}

float height_to_tex(float h, float atm_h) {
	return pow(h/atm_h, 0.5f);
}

float tex_to_height(float u, float atm_h) {
	return u*u*atm_h;
}

float view_to_tex(float c_v, float h, float rad_e) {
	float c_h = -sqrt(h*(2*rad_e+h))/(rad_e+h);
	if (c_v > c_h)
		return 0.5 + 0.5f*pow((c_v-c_h)/(1-c_h), 0.2f);
	else
		return 0.5 - 0.5f*pow((c_h-c_v)/(1+c_h), 0.2f);
}

float tex_to_view(float u, float h, float rad_e) {
	float c_h = -sqrt(h*(2*rad_e+h))/(rad_e+h);
	if (u > 0.5) {
		float tmp = 2*(u - 0.5);
		return c_h + tmp*tmp*tmp*tmp*tmp*(1 - c_h);
	} else {
		float tmp = -2*(u - 0.5);
		return c_h - tmp*tmp*tmp*tmp*tmp*(1+c_h);
	}
}

float sun_to_tex(float c_s) {
	return 0.5*(atan(max(c_s, -0.1975)*tan(1.26*1.1))/1.1 + (1-0.26));
}

float tex_to_sun(float u) {
	return tan(1.1*(2*u-1+0.26))/tan(1.26*1.1);
}

float tex_to_view_transmittance(float u) {
	return 2*u-1;
}

float view_to_tex_transmittance(float cos_v) {
	return (cos_v+1)/2.0f;
}

float tex_to_height_transmittance(float u, float h_atm) {
	return u*h_atm;
}

float height_to_tex_transmittance(float h, float h_atm) {
	return h/h_atm;
}

vec3 _fetch_transmittance(float height, float view_cos, float rad_e, float atmosphere_height, vec2 transmittance_resolution, sampler2D transmittance_texture) {
	float x = height_to_tex(height, atmosphere_height);
	float y = view_to_tex(view_cos, height, rad_e);

	return texture(transmittance_texture, tex_address_shifted(vec2(x, y), transmittance_resolution)).xyz;
}

// would be nicer to write using genType.
vec3 fetch_transmittance(vec2 from, float view_cos, float rad_e, float atmosphere_height, vec2 transmittance_resolution, sampler2D transmittance_texture) {
	float height = height(from, rad_e);
	return _fetch_transmittance(height, view_cos, rad_e, atmosphere_height, transmittance_resolution, transmittance_texture);
}

vec3 fetch_transmittance(vec3 from, float view_cos, float rad_e, float atmosphere_height, vec2 transmittance_resolution, sampler2D transmittance_texture) {
	float height = height(from, rad_e);
	return _fetch_transmittance(height, view_cos, rad_e, atmosphere_height, transmittance_resolution, transmittance_texture);
}

vec3 transmittance_from_density(float dens_m, float dens_r, float dens_o, vec3 extcoeff_m, vec3 extcoeff_r, vec3 extcoeff_o) {
	return exp(-(dens_m*extcoeff_m + dens_r*extcoeff_r + dens_o*extcoeff_o));
}

// @param view is normalized, points away from cam.
// @param light_in is normalized, points away from light source.
//
// @return vec4, rgb are rayleigh, mie can be approximated.
vec4 single_scattering(float h, vec2 view, vec2 light_in, float r_scale_height, float m_scale_height, vec3 scoeff_r, vec3 scoeff_m, vec3 extcoeff_r, vec3 extcoeff_m, vec3 extcoeff_o, float rad_e, float rad_a, float atmosphere_height, vec2 transmittance_resolution, sampler2D transmittance_texture) {
	// pos is height from earth-center.
	// (assume h is height from earth-surface);
	vec2 pa = vec2(0, h+rad_e);
	// intersection with earth or atmosphere.
	vec2 pb = atmosphere_earth_intersect(pa, view, rad_e, rad_a);
	// if (abs(length(pb) - RAD_E) < 1)
	// 	debugPrintfEXT("%f", length(pb));
	if (pb.x == INFINITY || isnan(pb.x)) {
		// no intersection with atmosphere.
		return vec4(0,0,0,0);
	}

	// calculate inscattering at pa (for prev_insc_*):
	vec3 insc_m_prev;
	vec3 insc_r_prev;
	float dens_m_prev = density_m(height(pa, rad_e), m_scale_height);
	float dens_r_prev = density_r(height(pa, rad_e), r_scale_height);
	float dens_o_prev = density_o(height(pa, rad_e), r_scale_height);

	float pc_x = concentric_circle_intersect_x(pa, -light_in, rad_e);
	if (pc_x != INFINITY) {
		// in earths shadow, no light from sun arrives.
		insc_m_prev = vec3(0,0,0);
		insc_r_prev = vec3(0,0,0);
	} else {
		// -vec.y is zenith-cosine for unit vectors.
		vec3 transmittance = fetch_transmittance(pa, -light_in.y, rad_e, atmosphere_height, transmittance_resolution, transmittance_texture);
		// vec3 transmittance = transmittance(pa, pa-light_in*pc_x, r_scale_height, m_scale_height, extcoeff_r, extcoeff_m, rad_e);
		// debugPrintfEXT("%v3f :: %v3f", transmittance, tpap);

		// sum contains only density at pa so far.
		insc_m_prev = dens_m_prev*transmittance;
		insc_r_prev = dens_r_prev*transmittance;
	}

	vec3 insc_m_sum = vec3(0,0,0);
	vec3 insc_r_sum = vec3(0,0,0);
	float dens_m_sum = 0;
	float dens_r_sum = 0;
	float dens_o_sum = 0;

	// will probably not split evenly, store p so we can integrate between
	// that last p and pb.
	int step_count = int(length(pa-pb)/SCATTERING_STEP_LENGTH);
	// declare outside loop so it's accessible after it.
	// init with pa so even step_count of 0 yields correct results.
	vec2 p = pa;
	for (int i = 1; i <= step_count; ++i) {
		p = pa + i*SCATTERING_STEP_LENGTH*view;

		// find current inscattering.
		vec3 insc_m;
		vec3 insc_r;

		// calculate density integral from pa to p.
		float dens_m = density_m(height(p, rad_e), m_scale_height);
		float dens_r = density_r(height(p, rad_e), r_scale_height);
		float dens_o = density_o(height(p, rad_e), r_scale_height);

		dens_m_sum += (dens_m_prev + dens_m)/2.0f * SCATTERING_STEP_LENGTH;
		dens_r_sum += (dens_r_prev + dens_r)/2.0f * SCATTERING_STEP_LENGTH;
		dens_o_sum += (dens_o_prev + dens_o)/2.0f * SCATTERING_STEP_LENGTH;

		dens_m_prev = dens_m;
		dens_r_prev = dens_r;
		dens_o_prev = dens_o;

		pc_x = concentric_circle_intersect_x(p, -light_in, rad_e);
		if (pc_x != INFINITY) {
			// in earths shadow, no light from sun arrives.
			insc_m = vec3(0,0,0);
			insc_r = vec3(0,0,0);
		} else {
			pc_x = concentric_circle_intersect_x(p, -light_in, rad_a);
			// cannot really happen, but rounding...
			// p should be right at the border of the atmosphere to not have a
			// positive intersection. Set pc_x accordingly.
			if (pc_x == INFINITY)
				pc_x = 0;

			vec3 tpap = transmittance_from_density(dens_m_sum, dens_r_sum, dens_o_sum, extcoeff_m, extcoeff_r, extcoeff_o);
			// vec3 tppc = transmittance(p, pc, r_scale_height, m_scale_height, extcoeff_r, extcoeff_m, rad_e);
			vec3 tppc = fetch_transmittance(p, dot(normalize(p), -light_in), rad_e, atmosphere_height, transmittance_resolution, transmittance_texture);

			vec3 transmittance = tpap*tppc;
			insc_m = dens_m*transmittance;
			insc_r = dens_r*transmittance;
		}

		// debugPrintfEXT("%f : %v2f %f t: %v3f : %v3f", i, p, height(p), insc_m, transmittance);
		
		// if (insc_m == vec3(0,0,0))
		// 	debugPrintfEXT("%v2f : %v3f : %v3f", p, tpap, tppc);

		insc_m_sum += (insc_m_prev + insc_m)/2.0f*SCATTERING_STEP_LENGTH;
		insc_r_sum += (insc_r_prev + insc_r)/2.0f*SCATTERING_STEP_LENGTH;

		// if (insc_r_sum == vec3(0.0f))
		// 	debugPrintfEXT("%v2f : %v2f : %v2f : %v2f", pa, pb, p, pc);

		insc_m_prev = insc_m;
		insc_r_prev = insc_r;
	}
	// integrate from last p to pb.
	vec3 insc_m;
	vec3 insc_r;

	float dens_m = density_m(height(pb, rad_e), m_scale_height);
	float dens_r = density_r(height(pb, rad_e), r_scale_height);
	float dens_o = density_o(height(pb, rad_e), r_scale_height);

	dens_m_sum += (dens_m_prev + dens_m)/2.0f * length(p - pb);
	dens_r_sum += (dens_r_prev + dens_r)/2.0f * length(p - pb);
	dens_o_sum += (dens_o_prev + dens_o)/2.0f * length(p - pb);

	pc_x = concentric_circle_intersect_x(pb, -light_in, rad_e);
	if (pc_x != INFINITY) {
		// in earths shadow, no light from sun arrives.
		insc_m = vec3(0,0,0);
		insc_r = vec3(0,0,0);
	} else {
		pc_x = concentric_circle_intersect_x(pb, -light_in, rad_a);
		// cannot really happen, but rounding...
		// pb should be right at the border of the atmosphere to not have a
		// positive intersection. Set pc_x accordingly.
		if (pc_x == INFINITY)
			pc_x = 0;

		// intersection with atmosphere in direction of light.
		vec2 pc = pb - light_in*pc_x;

		vec3 tpapb = transmittance_from_density(dens_m_sum, dens_r_sum, dens_o_sum, extcoeff_m, extcoeff_r, extcoeff_o);
		vec3 tpbpc = fetch_transmittance(pb, dot(normalize(pb), -light_in), rad_e, atmosphere_height, transmittance_resolution, transmittance_texture);

		vec3 transmittance = tpapb*tpbpc;
		insc_m = dens_m*transmittance;
		insc_r = dens_r*transmittance;
	}

	// distance from last p in loop.
	insc_m_sum += (insc_m_prev + insc_m)/2.0f*length(p - pb);
	insc_r_sum += (insc_r_prev + insc_r)/2.0f*length(p - pb);

	// if (h > 18 && h < 25 && view.y < -0.001 && view.y > -.01) {
	// 	debugPrintfEXT("%d : %v3f : %v3f : %f", step_count, insc_m_sum*scoeff_m.r/(4.0f*pi), insc_r_sum*scoeff_r/(4.0f*pi), length(p - pb));
	// 	/* insc_r_sum = vec3(1000); */
	// 	/* insc_m_sum = vec3(1000); */
	// }

	// according to 4.7.5
	return vec4(insc_r_sum*scoeff_r, insc_m_sum.r*scoeff_m.r)/(4.0f*pi);
}

// vec4 single_scattering2(float h, vec2 view, vec2 light_in, float r_scale_height, float m_scale_height, vec3 scoeff_r, vec3 scoeff_m, vec3 extcoeff_r, vec3 extcoeff_m, float rad_e, float rad_a, float atmosphere_height) {
// 	// pos is height from earth-center.
// 	// (assume h is height from earth-surface);
// 	vec2 pa = vec2(0, h+rad_e);
// 	// intersection with earth or atmosphere.
// 	vec2 pb = atmosphere_earth_intersect(pa, view, rad_e, rad_a);
// 	// if (abs(length(pb) - RAD_E) < 1)
// 	// 	debugPrintfEXT("%f", length(pb));
// 	if (pb.x == INFINITY || isnan(pb.x)) {
// 		// no intersection with atmosphere.
// 		return vec4(0,0,0,0);
// 	}
// 
// 	// calculate inscattering at pa (for prev_insc_*):
// 	vec3 insc_m_prev;
// 	vec3 insc_r_prev;
// 
// 	float pc_x = concentric_circle_intersect_x(pa, -light_in, rad_e);
// 	if (pc_x != INFINITY) {
// 		// in earths shadow, no light from sun arrives.
// 		insc_m_prev = vec3(0,0,0);
// 		insc_r_prev = vec3(0,0,0);
// 	} else {
// 		// -vec.y is zenith-cosine for unit vectors.
// 		// vec3 transmittance = fetch_transmittance(pa, -light_in.y, rad_e, atmosphere_height, transmittance_resolution, transmittance_texture);
// 		vec3 transmittance = transmittance(pa, pa-light_in*pc_x, r_scale_height, m_scale_height, extcoeff_r, extcoeff_m, rad_e);
// 		// debugPrintfEXT("%v3f :: %v3f", transmittance, tpap);
// 
// 		insc_m_prev = density_m(height(pa, rad_e), m_scale_height)*transmittance;
// 		insc_r_prev = density_r(height(pa, rad_e), r_scale_height)*transmittance;
// 	}
// 
// 	vec3 insc_m_sum = vec3(0,0,0);
// 	vec3 insc_r_sum = vec3(0,0,0);
// 	// will probably not split evenly, store p so we can integrate between
// 	// that last p and pb.
// 	int step_count = int(length(pa-pb)/SCATTERING_STEP_LENGTH);
// 	// declare outside loop so it's accessible after it.
// 	// init with pa so even step_count of 0 yields correct results.
// 	vec2 p = pa;
// 
// 	for (int i = 1; i <= step_count; ++i) {
// 		p = pa + i*SCATTERING_STEP_LENGTH*view;
// 
// 		// find current inscattering.
// 		vec3 insc_m;
// 		vec3 insc_r;
// 		pc_x = concentric_circle_intersect_x(p, -light_in, rad_e);
// 		if (pc_x != INFINITY) {
// 			// in earths shadow, no light from sun arrives.
// 			insc_m = vec3(0,0,0);
// 			insc_r = vec3(0,0,0);
// 		} else {
// 			pc_x = concentric_circle_intersect_x(p, -light_in, rad_a);
// 			// cannot really happen, but rounding...
// 			// p should be right at the border of the atmosphere to not have a
// 			// positive intersection. Set pc_x accordingly.
// 			if (pc_x == INFINITY)
// 				pc_x = 0;
// 
// 			// intersection with atmosphere.
// 			vec2 pc = p - light_in*pc_x;
// 
// 			vec3 tpap = transmittance(pa, p, r_scale_height, m_scale_height, extcoeff_r, extcoeff_m, rad_e);
// 			vec3 tppc = transmittance(p, pc, r_scale_height, m_scale_height, extcoeff_r, extcoeff_m, rad_e);
// 			// vec3 tppc = fetch_transmittance(p, dot(normalize(p), -light_in), rad_e, atmosphere_height, transmittance_resolution, transmittance_texture);
// 
// 			vec3 transmittance = tpap*tppc;
// 			insc_m = density_m(height(p, rad_e), m_scale_height)*transmittance;
// 			insc_r = density_r(height(p, rad_e), r_scale_height)*transmittance;
// 		}
// 
// 		// debugPrintfEXT("%f : %v2f %f t: %v3f : %v3f", i, p, height(p), insc_m, transmittance);
// 		
// 		// if (insc_m == vec3(0,0,0))
// 		// 	debugPrintfEXT("%v2f : %v3f : %v3f", p, tpap, tppc);
// 
// 		insc_m_sum += (insc_m_prev + insc_m)/2.0f*SCATTERING_STEP_LENGTH;
// 		insc_r_sum += (insc_r_prev + insc_r)/2.0f*SCATTERING_STEP_LENGTH;
// 
// 		// if (insc_r_sum == vec3(0.0f))
// 		// 	debugPrintfEXT("%v2f : %v2f : %v2f : %v2f", pa, pb, p, pc);
// 
// 		insc_m_prev = insc_m;
// 		insc_r_prev = insc_r;
// 	}
// 	// integrate from last p to pb.
// 	vec3 insc_m;
// 	vec3 insc_r;
// 	pc_x = concentric_circle_intersect_x(pb, -light_in, rad_e);
// 	if (pc_x != INFINITY) {
// 		// in earths shadow, no light from sun arrives.
// 		insc_m = vec3(0,0,0);
// 		insc_r = vec3(0,0,0);
// 	} else {
// 		pc_x = concentric_circle_intersect_x(pb, -light_in, rad_a);
// 		// cannot really happen, but rounding...
// 		// pb should be right at the border of the atmosphere to not have a
// 		// positive intersection. Set pc_x accordingly.
// 		if (pc_x == INFINITY)
// 			pc_x = 0;
// 
// 		// intersection with atmosphere in direction of light.
// 		vec2 pc = pb - light_in*pc_x;
// 
// 		vec3 tpapb = transmittance(pa, pb, r_scale_height, m_scale_height, extcoeff_r, extcoeff_m, rad_e);
// 		vec3 tpbpc = transmittance(pb, pc, r_scale_height, m_scale_height, extcoeff_r, extcoeff_m, rad_e);
// 
// 		vec3 transmittance = tpapb*tpbpc;
// 		insc_m = density_m(height(pb, rad_e), m_scale_height)*transmittance;
// 		insc_r = density_r(height(pb, rad_e), r_scale_height)*transmittance;
// 	}
// 
// 	// distance from last p in loop.
// 	insc_m_sum += (insc_m_prev + insc_m)/2.0f*length(p - pb);
// 	insc_r_sum += (insc_r_prev + insc_r)/2.0f*length(p - pb);
// 
// 	// if (h > 18 && h < 25 && view.y < -0.001 && view.y > -.01) {
// 	// 	debugPrintfEXT("%d : %v3f : %v3f : %f", step_count, insc_m_sum*scoeff_m.r/(4.0f*pi), insc_r_sum*scoeff_r/(4.0f*pi), length(p - pb));
// 	// 	/* insc_r_sum = vec3(1000); */
// 	// 	/* insc_m_sum = vec3(1000); */
// 	// }
// 
// 	// according to 4.7.5
// 	return vec4(insc_r_sum*scoeff_r, insc_m_sum.r*scoeff_m.r)/(4.0f*pi);
// }

vec3 mie_from_rayleigh(vec4 r_rgb_m_r, vec3 scoeff_r, vec3 scoeff_m) {
	return r_rgb_m_r.r == 0 ? vec3(0,0,0) : (r_rgb_m_r.rgb * r_rgb_m_r.a * scoeff_r.r * scoeff_m / (r_rgb_m_r.r * scoeff_m.r * scoeff_r));
}

vec2 unit_vec_from_cos(float cos) {
	cos = min(cos, 1);
	float sin = sqrt(1-cos*cos);
	return vec2(sin, cos);
}

// map depth linearly between 0 and 1.
// "inspired" by https://stackoverflow.com/questions/51108596/linearize-depth
float linearize_depth(float d,float zNear,float zFar)
{
    return ((zFar * zNear / (zFar + d * (zNear - zFar))) - zNear) /(zFar-zNear);
}
