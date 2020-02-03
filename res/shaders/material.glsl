

const uint MaterialInfo_CastReflections = 1;
const uint MaterialInfo_CastRefractions = 2;
const uint MaterialInfo_NoCastShadows = 4;
const uint MaterialInfo_UseSpecular = 8;
const uint MaterialInfo_TexAlbedo = 16;
const uint MaterialInfo_TexNormal = 32;
const uint MaterialInfo_TexRoughness = 64;
const uint MaterialInfo_TexMetallic = 128;
const uint MaterialInfo_TexSpecular = 256;
const uint MaterialInfo_TexOpacity = 512;
const uint MaterialInfo_TexEmission = 1024;

struct Material {

	vec3 albedo;
	float metallic;

	vec3 ambient;
	float roughness;

	vec3 emissive;
	float transparency;

	uvec3 padding;
	uint materialInfo;
};