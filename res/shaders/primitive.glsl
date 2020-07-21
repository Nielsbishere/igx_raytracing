
const float noHit = 3.4028235e38;
const float epsilon = 1e-2;		//epsilon for offsets
const float delta = 1e-1;		//epsilon for calculations

const uint shadowHit = 0xFFFFFFFF;		//loc1D is set to this when a shadow hit
const uint noRayHit = 0xFFFFFFFF;		//loc1D is set to this when a primary didn't hit

//Define very often used types

struct Ray { 

	vec3 pos;
	float pad0; 

	vec3 dir;
	float pad1;
};

struct RayPayload {

	vec3 dir;				//Vector direction
	uint loc1D;				//1D location in buffer

	vec3 color;				//Color this ray contributes
	float maxDist;			//Maximum hit distance along the ray's axis
};

struct ShadowPayload {

	vec3 pos;
	uint loc1D;

	vec3 dir;
	float maxDist;
};

struct Triangle {

	vec3 p0;
	uint object;

	vec3 p1;
	uint material;

	vec3 p2;
	uint padding;
};

struct Cube {

	vec3 start;
	uint object;

	vec3 end;
	uint material;
};


//Ray intersections

bool rayIntersectSphere(const Ray r, const vec4 sphere, inout vec3 hit) {

	const vec3 dif = sphere.xyz - r.pos;
	const float t = dot(dif, r.dir);

	const vec3 Q = dif - t * r.dir;
	const float Q2 = dot(Q, Q);

	const bool outOfSphere = Q2 > sphere.w;
	const float hitT = t - sqrt(sphere.w - Q2);

	if(!outOfSphere && hitT >= 0 && hitT < hit.z) {
		hit.z = hitT;
		return true;
	}

	return false;
}

bool rayIntersectPlane(const Ray r, const vec4 plane, inout vec3 hit, inout vec3 n) {

	vec3 dir = normalize(plane.xyz);
	float dif = dot(r.dir, dir);

	float hitT = -(dot(r.pos, dir) + plane.w) / dif;

	//TODO: If dif == 0, nan

	if(hitT >= delta && hitT < hit.z) {
		hit.z = hitT;
		n = dif < 0 ? -dir : dir;
		return true;
	}

	return false;
}

bool rayIntersectTri(const Ray r, const Triangle tri, inout vec3 hit) {

	const vec3 p1_p0 = tri.p1 - tri.p0;
	const vec3 p2_p0 = tri.p2 - tri.p0;
	
	const vec3 h = cross(r.dir, p2_p0);
	const float a = dot(p1_p0, h);
	
	if (abs(a) < delta) return false;
	
	const float f = 1 / a;
	const vec3 s = r.pos - tri.p0;
	const float u = f * dot(s, h);
	
	if (u < 0 || u > 1) return false; 
	
	const vec3 q = cross(s, p1_p0);
	const float v = f * dot(r.dir, q);
	
	if (v < 0 || u + v > 1) return false;
	
	const float t = f * dot(p2_p0, q);
	
	if (t <= delta || t >= 1 / delta || t >= hit.z) return false;

	hit = vec3(u, v, t);
	return true;
}

bool rayIntersectCube(const Ray r, const Cube cube, inout vec3 hit) {

	vec3 revDir = 1 / r.dir;

	vec3 startDir = (cube.start - r.pos) * revDir;
	vec3 endDir = (cube.end - r.pos) * revDir;

	float tmin = max(max(min(startDir.x, endDir.x), min(startDir.y, endDir.y)), min(startDir.z, endDir.z));
	float tmax = min(min(max(startDir.x, endDir.x), max(startDir.y, endDir.y)), max(startDir.z, endDir.z));

	if(tmax < 0 || tmin > tmax || tmin > hit.z)
		return false;

	hit.z = tmin;
	return true;
}

bool sphereInPlane(const vec4 sphere, const vec4 plane) {
	return dot(plane.xyz, sphere.xyz) + plane.w <= sphere.w;
}