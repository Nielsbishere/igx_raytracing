
//Sample point on equirectangular texture

uvec2 packHalf4x16(const vec4 rgba) {
	return uvec2(packHalf2x16(rgba.rg), packHalf2x16(rgba.ba));
}

vec4 unpackHalf4x16(const uvec2 rgba16f) {
	return uvec4(unpackHalf2x16(rgba16f.x), unpackHalf2x16(rgba16f.y));
}

vec4 tosRGB(vec4 rgb) {

	const vec4 rgb2 = rgb * rgb;

	return 
			0.012522878 * rgb +
            0.682171111 * rgb2 +
            0.305306011 * rgb2 * rgb;
}

vec4 toRGB(vec4 srgb) {

	vec4 s1 = sqrt(srgb);
	vec4 s2 = sqrt(s1);
	vec4 s4 = sqrt(s2);

	return
		0.662002687 * s1 + 
		0.684122060 * s2 - 
		0.323583601 * s4 - 
		0.0225411470 * srgb;
}