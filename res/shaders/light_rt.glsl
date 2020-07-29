#include "light.glsl"

layout(binding=11, std430) readonly buffer ShadowOutput32 {
    uint shadowOutput32[];
};

layout(binding=11, std430) readonly buffer ShadowOutput64 {
    uint64_t shadowOutput64[];
};

bool didHit(uvec2 loc, uint lightId) {

	uvec2 res = uvec2(width, height);

	if(gl_SubGroupSizeARB == 32) { 
	
		//const uvec2 threads = uvec2(16, 2);
		const uvec2 shift = uvec2(4, 1);			//log2(threads)
		const uvec2 mask = uvec2(15, 1);			//threads - 1

		const uvec2 inTile = loc & mask;
		const uint tileShift = inTile.x | (inTile.y << shift.x);

		return (shadowOutput32[indexToLight(loc, res, lightId, shift, mask)] & (1 << tileShift)) == 0;
	}
	
	//const uvec2 threads = uvec2(16, 4);
	const uvec2 shift = uvec2(4, 2);			//log2(threads)
	const uvec2 mask = uvec2(15, 3);			//threads - 1

	const uvec2 inTile = loc & mask;
	const uint tileShift = inTile.x | (inTile.y << shift.x);

	return (shadowOutput64[indexToLight(loc, res, lightId, shift, mask)] & (1 << tileShift)) == 0;
}

vec3 shadeRT(
	const Material m,
	const vec3 pos,
	const vec3 n, 
	const vec3 v,
	const float NdotV,
	const vec3 reflected,
	const uvec2 loc
) {
	const vec3 F0 = mix(vec3(0.04), m.albedo, m.metallic);

	const vec3 kS = fresnelSchlickRoughness(F0, NdotV, m.metallic, m.albedo, m.roughness);
	const vec3 kD = (1 - kS) * (1 - m.metallic);

	vec3 lighting = vec3(0, 0, 0);

	for(uint i = 0; i < lights.length(); ++i)
		if(didHit(loc, i))
			lighting += shadeLight(F0, m, lights[i], pos, n, v, NdotV);

	return (m.ambient + kD / pi) * m.albedo + kS * reflected + lighting + m.emissive;
}

vec3 shadeHitRT(Ray ray, Hit hit, vec3 reflection, const uvec2 loc) {

	if(hit.hitT == noHit)
		return vec3(sampleSkybox(ray.dir));

	const vec3 position = ray.pos + ray.dir * hit.hitT;
	
	const vec3 v = ray.dir;
	const float NdotV = max(dot(v, -hit.normal), 0);

	return shadeRT(materials[hit.material], position, hit.normal, v, NdotV, reflection, loc);
}

vec3 shadeHitFinalRecursionRT(Ray ray, Hit hit, const uvec2 loc) {

	if(hit.hitT == noHit)
		return vec3(sampleSkybox(ray.dir));

	const vec3 position = ray.pos + ray.dir * hit.hitT;
	
	const vec3 v = ray.dir;
	const float NdotV = max(dot(v, -hit.normal), 0);

	const vec3 reflected = sampleSkybox(reflect(v, hit.normal));

	return shadeRT(materials[hit.material], position, hit.normal, v, NdotV, reflected, loc);
}