
float luminance(const vec3 color) {
	return dot(color, vec3(0.299, 0.587, 0.114));
}

vec3 fromSRGB(const vec3 color) {
	return
		color * 0.012522878 +
		color * color * 0.682171111 +
		color * color * color * 0.305306011;
}

vec3 exposureMapping(const vec3 color, const float exposure) {
	return vec3(1, 1, 1) - exp(-color * exposure);
}