#include "utils.glsl"
#include "material.glsl"

const float minRoughness = 0.01;
const float specularEpsilon = 0.001;

const uint LightType_Directional = 0;
const uint LightType_Point = 1;
//const uint LightType_Spot = 2;		TODO:
//const uint LightType_Area = 3;		TODO:

struct Light {

	vec3 pos;
	float rad;

	vec3 dir;
	uint type;

	vec3 col;
	float angle;

};

vec3 lambert(const vec3 diffuse, const Light light, const vec3 n, const vec3 l, const float dist) {
	const float NdotL = max(dot(n, l), 0);
	return diffuse * light.col * dist * NdotL;
}

//Lighting with the metallic workflow

//Normal distribution function (GGX Trowbridge-Reitz)
float ndfGGX(const vec3 n, const vec3 h, const float roughness) {
	
	const float alpha = roughness * roughness;
	const float a2 = alpha * alpha;

	const float NdotH = max(dot(n, h), 0);

	float denom = NdotH * NdotH * (a2 - 1) + 1;
	denom *= denom * pi;

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

//Fresnel approximation (Schlick)
vec3 fresnelSchlick(const vec3 F0, const vec3 h, const vec3 v) {
	return F0 + (1 - F0) * pow(1 - dot(h, v), 5);
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

	const vec3 kS = D * F * G / (4 * NdotL * NdotV + specularEpsilon);
	const vec3 kD = (1 - F) * (1 - metallic);

	const vec3 color = kD * albedo + kS;

	return color * light.col * invSquareDist * NdotL;
}

//Per light shading

layout(binding=7, std140) readonly buffer Lights {
	Light lights[];
};

vec3 shadeLight(
	const vec3 F0,
	const Material m,
	const Light light, 
	const vec3 pos,
	const vec3 n,
	const vec3 v,
	const float NdotV
) {

	vec3 l = light.dir;
	float invSquareDist = 1;
	float maxDist = noHit;

	if(light.type == LightType_Point) {

		l = pos - light.pos;
		const float dst = length(l);
		invSquareDist = max(1 - dst / light.rad, 0);		//TODO: This should be dst^2

		//Calculate params for point light shadows

		maxDist = dst;				//TODO: This is along the direction of the light, it should be along the distance of the ray
	}

	l = normalize(l);

	//Calculate color

	float k = m.roughness + 1;
	k *= k / 8;
	
	const float NdotL = max(dot(n, l), 0);

	return cookTorrance(F0, m.albedo, light, n, l, v, NdotV, invSquareDist, m.roughness, m.metallic, k, NdotL);
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
	const vec3 F0 = mix(vec3(0.04), m.albedo, m.metallic);

	const vec3 kS = fresnelSchlickRoughness(F0, NdotV, m.metallic, m.albedo, m.roughness);
	const vec3 kD = (1 - kS) * (1 - m.metallic);

	vec3 lighting = vec3(0, 0, 0);

	for(uint i = 0; i < lights.length(); ++i)
		lighting += shadeLight(F0, m, lights[i], pos, n, v, NdotV);

	return (m.ambient + kD / pi) * m.albedo + kS * reflected + lighting + m.emissive;
}

layout(binding=3, std140) readonly buffer Materials {
	Material materials[];
};

vec3 shadeHit(Ray ray, Hit hit, vec3 reflection, vec3 missColor) {

	if(hit.hitT == noHit)
		return vec3(missColor);

	const vec3 position = ray.pos + ray.dir * hit.hitT;
	
	const vec3 v = normalize(position - eye);
	const float NdotV = max(dot(-hit.normal, v), 0);

	return shade(materials[hit.material], position, hit.normal, v, NdotV, reflection);
}

vec3 shadeRay(Ray ray, vec3 reflection, vec3 missColor) {
	Hit hit = traceGeometry(ray);
	return shadeHit(ray, hit, reflection, missColor);
}