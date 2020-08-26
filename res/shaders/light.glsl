#ifndef LIGHT
#define LIGHT
#include "utils.glsl"
#include "trace.glsl"

const float minRoughness = 0.01;
const float specularEpsilon = 0.001;

const uint LightType_Directional = 0;
//const uint LightType_Spot = 1;		TODO:
const uint LightType_Point = 2;
//const uint LightType_Area = 3;		TODO:

vec3 lambert(const vec3 diffuse, const Light light, const vec3 n, const vec3 l, const float dist) {
	const float NdotL = max(dot(n, l), 0);
	return diffuse * unpackColor3(light.colorType) * dist * NdotL;
}

//Lighting with the metallic workflow

//Normal distribution function (GGX Trowbridge-Reitz)
float ndfGGX(const vec3 n, const vec3 h, const float roughness) {
	
	const float alpha = roughness * roughness;
	const float a2 = alpha * alpha;

	const float NdotH = max(dot(n, h), 0);

	float denom = NdotH * NdotH * (a2 - 1) + 1;
	denom *= denom * pi;

	if(denom == 0) return 0;

	return a2 / denom;
}

//Geometry function (schlick GGX)
float geomSchlickGGX(const float NdotV, const float k) {
	return NdotV / (NdotV * (1 - k) + k);
}

//Geometry function (schlick GGX smith)
float geomSmith(const float NdotV, const float NdotL, const float k) {
	return geomSchlickGGX(NdotV, k) * geomSchlickGGX(NdotL, k);
}

float pow5(float f) {
	float f2 = f * f;
	return f2 * f2 * f;
}

//Fresnel approximation (Schlick)
vec3 fresnelSchlick(const vec3 F0, const vec3 h, const vec3 v) {
	return F0 + (1 - F0) * pow5(1 - max(dot(h, v), 0));
}

//Fresnel approximation (Schlick roughness)
vec3 fresnelSchlickRoughness(const vec3 F0, const float NdotV, const float metallic, const vec3 albedo, const float roughness) {
	return F0 + (max(F0, vec3(1 - roughness)) - F0) * pow(1 - NdotV, 5);
}

//Cook-torrance brdf

vec3 cookTorrance(
	const vec3 F0,
	const vec3 albedo,
	const Light light, 
	const vec3 n,
	const vec3 l,
	const vec3 v,
	const float NdotV, 
	const float invSquareDist,
	const float roughness,
	const float metallic,
	const float k,
	const float NdotL
) {

	const vec3 h = normalize(l + v);

	const float D = ndfGGX(n, h, max(roughness, minRoughness));
	const float G = geomSmith(NdotV, NdotL, k);
	const vec3 F = fresnelSchlick(F0, h, v);

	const float denom = 4 * NdotL * NdotV + specularEpsilon;
	const vec3 num = D * G * F;

	const vec3 kS = num / denom;
	const vec3 kD = (1 - F) * (1 - metallic);

	const vec3 color = kD * albedo + kS;

	return color * unpackColor3(light.colorType) * invSquareDist * NdotL;
}

//Per light shading

vec3 getDirToLight(const Light light, const vec3 pos, inout float brightness, inout float maxDist) {

	vec3 l;
	brightness = 1;
	maxDist = noHit;

	//Point

	if(unpackColorA(light.colorType) == LightType_Point) {

		l = pos - light.pos;
		const float dst = length(l);

		const float normDist = dst / unpackHalf2x16(light.radOrigin).x;

		brightness = max(1 - normDist * normDist, 0);

		//Calculate params for point light shadows

		maxDist = dst;				//TODO: This is along the direction of the light, it should be along the distance of the ray
	}

	//Directional

	else l = decodeNormal(light.dir);

	return normalize(l);
}

vec3 shadeLight(
	const vec3 F0,
	const vec3 albedo,
	const float roughness,
	const float metallic,
	const Light light, 
	const vec3 pos,
	const vec3 n,
	const vec3 v,
	const float NdotV
) {

	float brightness, maxDist;
	vec3 l = getDirToLight(light, pos, brightness, maxDist);

	//Calculate color

	float k = roughness + 1;
	k *= k / 8;
	
	const float NdotL = max(dot(n, l), 0);

	return cookTorrance(F0, albedo, light, n, l, v, NdotV, brightness, roughness, metallic, k, NdotL);
}

//Per pixel shading

vec3 shade(
	const Material m,
	const vec3 pos,
	const vec3 n, 
	const vec3 v,
	const float NdotV,
	const vec3 reflected
) {

	const float roughness = unpackColorAUnorm(m.ambientRoughness);
	const vec3 ambient = unpackColor3(m.ambientRoughness);

	const float metallic = unpackColorAUnorm(m.albedoMetallic);
	const vec3 albedo = unpackColor3(m.albedoMetallic);

	const vec3 F0 = mix(vec3(0.04), albedo, metallic);

	const vec3 kS = fresnelSchlickRoughness(F0, NdotV, metallic, albedo, roughness);
	const vec3 kD = (1 - kS) * (1 - metallic);

	vec3 lighting = vec3(0, 0, 0);

	for(uint i = 0; i < lights.length(); ++i)
		lighting += shadeLight(F0, albedo, roughness, metallic, lights[i], pos, n, v, NdotV);

	const vec3 emissive = unpackColor3(m.emissive);

	return (ambient + kD / pi) * albedo + kS * reflected + lighting + emissive;
}

vec3 shadeHit(Ray ray, Hit hit, vec3 reflection) {

	if(hit.hitT == noHit)
		return vec3(sampleSkybox(ray.dir));

	const vec3 position = ray.pos + ray.dir * hit.hitT;
	
	const vec3 n = decodeNormalLQ(hit.normal);
	const vec3 v = ray.dir;
	const float NdotV = max(dot(v, -n), 0);

	const Material m = materials[materialIndices[hit.object]];

	return shade(m, position, n, v, NdotV, reflection);
}

vec3 shadeHitFinalRecursion(Ray ray, Hit hit) {

	if(hit.hitT == noHit)
		return vec3(sampleSkybox(ray.dir));

	const vec3 position = ray.pos + ray.dir * hit.hitT;
	
	const vec3 n = decodeNormalLQ(hit.normal);
	const vec3 v = ray.dir;
	const float NdotV = max(dot(v, -n), 0);

	const vec3 reflected = sampleSkybox(reflect(v, n));

	return shade(materials[materialIndices[hit.object]], position, n, v, NdotV, reflected);
}

uint indexToLight(uvec2 loc, uvec2 res, uint lightId, uvec2 shift, uvec2 mask) {

	const uvec2 tile = loc >> shift;
	uvec2 tiles = res >> shift;

	tiles += uvec2(notEqual(res & mask, uvec2(0)));		//ceil(res / threads)

	return tile.x + tile.y * tiles.x + lightId * tiles.y * tiles.x;
}

#endif