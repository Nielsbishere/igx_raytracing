#ifndef PRIMITIVE
#define PRIMITIVE

//Define very often used types

const float noHit = 3.4028235e38;
const uint noRayHit = 0xFFFFFFFF;

struct Hit {
	
	uvec2 normal;
	uvec2 rayDir;

	vec3 rayOrigin;
	float hitT;

	vec2 uv;
	uint object;
	uint primitive;

};

struct Ray { 
	vec3 pos;
	vec3 dir;
};

struct Triangle {
	vec3 p0;
	vec3 p1;
	vec3 p2;
};

struct Cube {
	vec3 start;
	vec3 end;
};

struct Light {

	vec3 pos;
	uint radOrigin;

	uvec2 dir;
	uvec2 colorType;	//colorType.y >> 16 = colorType, unpackColor(colorType).rgb = color

};

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

	uvec2 albedoMetallic;
	uvec2 ambientRoughness;

	uvec2 emissive;
	float transparency;
	uint materialInfo;

};

//Store 22-22-20 instead of 24
//Meaning we can almost represent a full float for x and y channels
//This is about 6.6 digits precision and 6 for the z channel

uvec2 encodeNormal(vec3 n) {

	n = n * 0.5 + 0.5;

	uvec3 multi = uvec3(n * (vec3(1 << 22, 1 << 22, 1 << 20) - 1));
	
	return uvec2(
		(multi.x << 10) | (multi.y >> 12),
		(multi.y << 20) | multi.z
	);
}

vec3 decodeNormal(uvec2 stored) {

	uvec3 unpacked = uvec3(
		stored.x >> 10,
		(stored.x << 22) | (stored.y >> 20),
		stored.y & ((1 << 20) - 1)
	);

	const vec3 div = 1 / (vec3(1 << 21, 1 << 21, 1 << 19) - 1);
	return vec3(unpacked) * div - 1;
}

//Allows packing 16-bit info into a

uvec2 packColor(vec3 col, uint a) {
	return uvec2(packHalf2x16(col.rg), packHalf2x16(vec2(col.b, 0)) | (a << 16));
}

uvec2 packColor(vec4 col) {
	return uvec2(packHalf2x16(col.rg), packHalf2x16(col.ba));
}

vec3 unpackColor3(uvec2 col) {
	return vec3(unpackHalf2x16(col.r), unpackHalf2x16(col.g).r);
}

vec4 unpackColor4(uvec2 col) {
	return vec4(unpackHalf2x16(col.r), unpackHalf2x16(col.g));
}

uint unpackColorA(uvec2 col) {
	return col.g >> 16;
}

//Ray intersections

bool rayIntersectSphere(const Ray r, const vec4 sphere, inout Hit hit, uint64_t obj, uint64_t prevObj) {

	const vec3 dif = sphere.xyz - r.pos;
	const float t = dot(dif, r.dir);

	const vec3 Q = dif - t * r.dir;
	const float Q2 = dot(Q, Q);

	const bool outOfSphere = Q2 > sphere.w;
	const float hitT = t - sqrt(sphere.w - Q2);

	if(!outOfSphere && obj != prevObj && hitT >= 0 && hitT < hit.hitT) {

		hit.hitT = hitT;

		const vec3 o = hitT * r.dir + r.pos;
		const vec3 normal = normalize(sphere.xyz - o);

		hit.normal = encodeNormal(normal);

		//Convert lat/long to uv
		//Limits of asin and atan are pi/2, so we divide by that to get normalized coordinates
		//We then * 0.5 + 0.5 to get a uv in that representation

		float latitude = asin(normal.z);
		float longitude = atan(normal.y / normal.x);

		if(isnan(longitude)) 
			longitude = 0;

		hit.uv = vec2(latitude, longitude) * (0.636619746685 * 0.5) + 0.5;

		return true;
	}

	return false;
}

bool rayIntersectPlane(const Ray r, const vec4 plane, inout Hit hit, uint64_t obj, uint64_t prevObj) {

	vec3 dir = normalize(plane.xyz);
	float dif = dot(r.dir, -dir);

	//TODO: dif == 0 shouldn't be a problem

	float hitT = -(dot(r.pos, -dir) + plane.w) / dif;

	if(hitT >= 0 && obj != prevObj && hitT < hit.hitT) {

		hit.hitT = hitT;
		hit.normal = encodeNormal(dif > 0 ? -dir : dir);

		vec3 o = hitT * r.dir + r.pos;

		vec3 planeX = cross(plane.xyz, vec3(0, 0, 1));
		vec3 planeZ = cross(plane.xyz, vec3(1, 0, 0));

		hit.uv = vec2(dot(o, planeX), dot(o, planeZ));

		return true;
	}

	return false;
}

bool rayIntersectTri(const Ray r, const Triangle tri, inout Hit hit, uint64_t obj, uint64_t prevObj) {

	const vec3 p1_p0 = tri.p1 - tri.p0;
	const vec3 p2_p0 = tri.p2 - tri.p0;
	
	const vec3 h = cross(r.dir, p2_p0);
	const float a = dot(p1_p0, h);
	
	if (abs(a) < 0) return false;
	
	const float f = 1 / a;
	const vec3 s = r.pos - tri.p0;
	const float u = f * dot(s, h);
	
	if (u < 0 || u > 1) return false; 
	
	const vec3 q = cross(s, p1_p0);
	const float v = f * dot(r.dir, q);
	
	if (v < 0 || u + v > 1) return false;
	
	const float t = f * dot(p2_p0, q);
	
	if (t <= 0 || obj == prevObj || t >= hit.hitT) return false;

	hit.uv = vec2(u, v);
	hit.hitT = t;
	hit.normal = encodeNormal(normalize(cross(tri.p1 - tri.p0, tri.p2 - tri.p0)) * -sign(a));

	return true;
}

bool rayIntersectCube(const Ray r, const Cube cube, inout Hit hit, uint64_t obj, uint64_t prevObj) {

	vec3 revDir = 1 / r.dir;

	vec3 startDir = (cube.start - r.pos) * revDir;
	vec3 endDir = (cube.end - r.pos) * revDir;

	vec3 mi = min(startDir, endDir);
	vec3 ma = max(startDir, endDir);

	float tmin = max(max(mi.x, mi.y), mi.z);
	float tmax = min(min(ma.x, ma.y), ma.z);

	if(tmax < 0 || tmin > tmax || tmin > hit.hitT || obj == prevObj)
		return false;

	//Determine which plane it's on (of the x, y or z plane)
	//and then make sure the sign is maintained to make it into a side

	vec3 pos = (r.dir * tmin + r.pos) - cube.start;
	pos /= cube.end;

	if(tmin == mi.x) {
		int isLeft = int(mi.x == startDir.x);
		hit.normal = encodeNormal(vec3(isLeft * 2 - 1, 0, 0));
		hit.uv = pos.yz;
	}

	else if(tmin == mi.y) {
		int isDown = int(mi.y == startDir.y);
		hit.normal = encodeNormal(vec3(0, isDown * 2 - 1, 0));
		hit.uv = pos.xz;
	}

	else {
		int isBack = int(mi.z == startDir.z);
		hit.normal = encodeNormal(vec3(0, 0, isBack * 2 - 1));
		hit.uv = pos.xy;
	}

	//

	hit.hitT = tmin;
	return true;
}

bool sphereInPlane(const vec4 sphere, const vec4 plane) {
	return dot(plane.xyz, sphere.xyz) + plane.w <= sphere.w;
}

#endif