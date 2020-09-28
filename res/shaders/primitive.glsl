#ifndef PRIMITIVE
#define PRIMITIVE

//Define very often used types

const float noHit = 3.4028235e38;
const uint noRayHit = 0xFFFFFFFF;

struct Hit {
	
	vec3 rayDir;
	float hitT;

	vec2 uv;
	uint object;
	uint normal;

};

struct Ray { 
	vec3 pos;
	vec3 dir;
};

struct Triangle {
	float p0[3];
	float p1[3];
	float p2[3];
};

struct Cube {
	vec2 xy0;
	vec2 z0_x1;
	vec2 yz1;
};

struct Light {

	vec3 pos;
	uint radOrigin;

	uvec2 dir;
	uvec2 colorType;	//colorType.y >> 16 = colorType, unpackColor3(colorType) = color

};

struct Frustum {
	vec4 planes[6];
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

//We store a rgba16s with padding into b

uvec2 encodeNormal(vec3 n) {
	uvec3 nh = uvec3((normalize(n) * 0.5 + 0.5) * 65535);
	return uvec2((nh.x << 16) | nh.y, nh.z);
}

vec3 decodeNormal(uvec2 stored) {
	uvec3 nh = uvec3(stored.x >> 16, stored.x & 65535, stored.y);
	return vec3(nh) / 65535 * 2 - 1;
}

//10 bits per channel (1 / 512 precision; 0.001953125x)

uint encodeNormalLQ(vec3 n) {
	uvec3 nh = uvec3((normalize(n) * 0.5 + 0.5) * 1023);
	return (nh.x << 20) | (nh.y << 10) | nh.z;
}

vec3 decodeNormalLQ(uint n) {
	uvec3 nh = uvec3(n >> 20, n >> 10, n) & 1023;
	return vec3(nh) / 1023 * 2 - 1;
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

float unpackColorAUnorm(uvec2 col) {
	return float(col.g >> 16) / 65535.0;
}

//Ray intersections

vec2 rayIntersectionsSphere(const Ray r, const vec4 sphere) {

	const vec3 dif = sphere.xyz - r.pos;
	const float t = dot(dif, r.dir);

	const vec3 Q = dif - t * r.dir;
	const float Q2 = dot(Q, Q);
	const float R2 = sphere.w * sphere.w;

	const bool outOfSphere = Q2 > R2;

	if(outOfSphere)
		return vec2(noHit);

	const float D = sqrt(R2 - Q2);
	return vec2(t - D, t + D);
}

bool rayIntersectSphere(const Ray r, const vec4 sphere, inout Hit hit, uint64_t obj, uint64_t prevObj) {

	const vec3 dif = sphere.xyz - r.pos;
	const float t = dot(dif, r.dir);

	const vec3 Q = dif - t * r.dir;
	const float Q2 = dot(Q, Q);
	const float R2 = sphere.w * sphere.w;

	const bool outOfSphere = Q2 > R2;
	const float hitT = t - sqrt(R2 - Q2);

	if(!outOfSphere && obj != prevObj && hitT >= 0 && hitT < hit.hitT) {

		hit.hitT = hitT;

		const vec3 o = hitT * r.dir + r.pos;
		const vec3 normal = normalize(sphere.xyz - o);

		hit.normal = encodeNormalLQ(normal);

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
		hit.normal = encodeNormalLQ(dif > 0 ? -dir : dir);

		vec3 o = hitT * r.dir + r.pos;

		vec3 planeX = cross(plane.xyz, vec3(0, 0, 1));
		vec3 planeZ = cross(plane.xyz, vec3(1, 0, 0));

		hit.uv = vec2(dot(o, planeX), dot(o, planeZ));

		return true;
	}

	return false;
}

bool rayIntersectTri(const Ray r, const Triangle tri, inout Hit hit, uint64_t obj, uint64_t prevObj) {

	const vec3 p0 = vec3(tri.p0[0], tri.p0[1], tri.p0[2]);
	const vec3 p1 = vec3(tri.p1[0], tri.p1[1], tri.p1[2]);
	const vec3 p2 = vec3(tri.p2[0], tri.p2[1], tri.p2[2]);

	const vec3 p1_p0 = p1 - p0;
	const vec3 p2_p0 = p2 - p0;
	
	const vec3 h = cross(r.dir, p2_p0);
	const float a = dot(p1_p0, h);
	
	if (abs(a) < 0) return false;
	
	const float f = 1 / a;
	const vec3 s = r.pos - p0;
	const float u = f * dot(s, h);
	
	if (u < 0 || u > 1) return false; 
	
	const vec3 q = cross(s, p1_p0);
	const float v = f * dot(r.dir, q);
	
	if (v < 0 || u + v > 1) return false;
	
	const float t = f * dot(p2_p0, q);
	
	if (t <= 0 || obj == prevObj || t >= hit.hitT) return false;

	hit.uv = vec2(u, v);
	hit.hitT = t;
	hit.normal = encodeNormalLQ(normalize(cross(p1 - p0, p2 - p0)) * -sign(a));

	return true;
}

bool rayIntersectCube(const Ray r, const Cube cube, inout Hit hit, uint64_t obj, uint64_t prevObj) {

	vec3 revDir = 1 / r.dir;

	vec3 start = vec3(cube.xy0, cube.z0_x1.x);
	vec3 end = vec3(cube.z0_x1.y, cube.yz1);

	vec3 startDir = (start - r.pos) * revDir;
	vec3 endDir = (end - r.pos) * revDir;

	vec3 mi = min(startDir, endDir);
	vec3 ma = max(startDir, endDir);

	float tmin = max(max(mi.x, mi.y), mi.z);
	float tmax = min(min(ma.x, ma.y), ma.z);

	if(tmax < 0 || tmin > tmax || tmin > hit.hitT || obj == prevObj)
		return false;

	//Determine which plane it's on (of the x, y or z plane)
	//and then make sure the sign is maintained to make it into a side

	vec3 pos = (r.dir * tmin + r.pos) - start;
	pos /= end;

	if(tmin == mi.x) {
		int isLeft = int(mi.x == startDir.x);
		hit.normal = encodeNormalLQ(vec3(isLeft * 2 - 1, 0, 0));
		hit.uv = pos.yz;
	}

	else if(tmin == mi.y) {
		int isDown = int(mi.y == startDir.y);
		hit.normal = encodeNormalLQ(vec3(0, isDown * 2 - 1, 0));
		hit.uv = pos.xz;
	}

	else {
		int isBack = int(mi.z == startDir.z);
		hit.normal = encodeNormalLQ(vec3(0, 0, isBack * 2 - 1));
		hit.uv = pos.xy;
	}

	//

	hit.hitT = tmin;
	return true;
}

//Sphere in plane means; the sphere intersects with the plane

bool sphereInPlane(const vec4 sphere, const vec4 plane) {
	return dot(plane.xyz, sphere.xyz) + plane.w <= sphere.w;
}

//Sphere inside plane means; if the sphere is on the positive side of the plane
//Used for things like frustum checks

bool sphereInsidePlane(const vec4 sphere, const vec4 plane) {
	return dot(plane.xyz, sphere.xyz) + plane.w > -sphere.w;
}

//If the sphere is intersecting or inside of the frustum

bool sphereInsideFrustum(const vec4 sphere, const Frustum frustum) {

	for(uint i = 0; i < 2; ++i)
		if(!sphereInsidePlane(sphere, frustum.planes[i]))
			return false;

	return true;
}

#endif