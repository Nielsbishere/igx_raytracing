
const float pi = 3.1415926535;

//Conversion functions for memory

uint to1D(uvec2 offset, uvec2 size) {
	return offset.x + offset.y * size.x;
}

uint to1D(uvec3 offset, uvec3 size) {
	return offset.x + offset.y * size.x + offset.z * size.y * size.x;
}

//Sample point on equirectangular texture

const vec2 invAtan = vec2(0.1591, 0.3183);

vec2 sampleEquirect(vec3 n) {
	const vec2 uv = vec2(atan(n.x, n.z), asin(n.y * -1)) * invAtan;
	return uv + 0.5;
}

//Spheremap transform normal compression
//aras-p.info/texts/CompactNormalStorage.html#method04spheremap

vec2 compressNormal(vec3 n) {
	const float p = sqrt(n.z * 8 + 8);
    return n.xy / p + 0.5;
}

vec3 decompressNormal(vec2 dir) {

    const vec2 fenc = dir * 4 - 2;
    const float f = dot(fenc, fenc);
    const float g = sqrt(1 - f / 4);

    return vec3(
		fenc * g,
		1 - f / 2
	);
}
