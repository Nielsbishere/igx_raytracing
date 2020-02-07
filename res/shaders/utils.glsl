
const float pi = 3.1415926535;

//Sample point on equirectangular texture

const vec2 invAtan = vec2(0.1591, 0.3183);

vec2 sampleEquirect(vec3 n) {
	const vec2 uv = vec2(atan(n.x, n.z), asin(n.y * -1)) * invAtan;
	return uv + 0.5;
}