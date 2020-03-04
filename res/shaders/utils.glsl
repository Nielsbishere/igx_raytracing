
const float pi = 3.1415926535;

//Sample point on equirectangular texture

const vec2 invAtan = vec2(0.1591, 0.3183);

vec2 sampleEquirect(const vec3 n) {
	const vec2 uv = vec2(atan(n.x, n.z), asin(n.y * -1)) * invAtan;
	return uv + 0.5;
}

uvec2 packHalf4x16(const vec4 rgba) {
	return uvec2(packHalf2x16(rgba.rg), packHalf2x16(rgba.ba));
}

vec4 unpackHalf4x16(const uvec2 rgba16f) {
	return uvec4(unpackHalf2x16(rgba16f.x), unpackHalf2x16(rgba16f.y));
}