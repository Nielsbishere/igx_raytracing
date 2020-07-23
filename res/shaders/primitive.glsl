
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

struct Hit {
	
	vec2 intersection;
	float hitT;
	uint material;

	vec3 normal;
	uint object;

};


//Ray intersections

bool rayIntersectSphere(const Ray r, const vec4 sphere, inout Hit hit) {

	const vec3 dif = sphere.xyz - r.pos;
	const float t = dot(dif, r.dir);

	const vec3 Q = dif - t * r.dir;
	const float Q2 = dot(Q, Q);

	const bool outOfSphere = Q2 > sphere.w;
	const float hitT = t - sqrt(sphere.w - Q2);

	if(!outOfSphere && hitT >= 0 && hitT < hit.hitT) {

		hit.hitT = hitT;

		const vec3 o = hitT * r.dir + r.pos;

		hit.normal = normalize(sphere.xyz - o);

		float latitude = asin(hit.normal.z);
		float longitude = atan(hit.normal.y / hit.normal.x);

		//Limits of asin and atan are pi/2, so we divide by that to get normalized coordinates
		//We then * 0.5 + 0.5 to get a uv in that representation
		//if lat == NaN && lon == NaN: x = 1 or x = -1

		hit.intersection = vec2(latitude, longitude) * (0.636619746685 * 0.5) + 2.5;

		return true;
	}

	return false;
}

bool rayIntersectPlane(const Ray r, const vec4 plane, inout Hit hit) {

	vec3 dir = normalize(plane.xyz);
	float dif = dot(r.dir, dir);

	//TODO: dif == 0 shouldn't be a problem

	float hitT = -(dot(r.pos, dir) + plane.w) / dif;

	if(hitT >= delta && hitT < hit.hitT) {

		hit.hitT = hitT;
		hit.normal = dif < 0 ? -dir : dir;

		vec3 o = hitT * r.dir + r.pos;

		vec3 planeX = cross(plane.xyz, vec3(0, 0, 1));
		vec3 planeZ = cross(plane.xyz, vec3(1, 0, 0));

		hit.intersection = fract(vec2(dot(o, planeX), dot(o, planeZ))) + vec2(int(dif < 0) << 1, 0) + 2;

		return true;
	}

	return false;
}

bool rayIntersectTri(const Ray r, const Triangle tri, inout Hit hit) {

	//TODO: Triangles act weird >:(

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
	
	if (t <= delta || t >= 1 / delta || t >= hit.hitT) return false;

	hit.intersection = vec2(u + (int(sign(a)) << 1), v) + 2;
	hit.hitT = t;
	return true;
}

bool rayIntersectCube(const Ray r, const Cube cube, inout Hit hit) {

	vec3 revDir = 1 / r.dir;

	vec3 startDir = (cube.start - r.pos) * revDir;
	vec3 endDir = (cube.end - r.pos) * revDir;

	vec3 mi = min(startDir, endDir);
	vec3 ma = max(startDir, endDir);

	float tmin = max(max(mi.x, mi.y), mi.z);
	float tmax = min(min(ma.x, ma.y), ma.z);

	if(tmax < 0 || tmin > tmax || tmin > hit.hitT)
		return false;

	//Determine which plane it's on (of the x, y or z plane)
	//and then make sure the sign is maintained to make it into a side

	vec3 pos = (r.dir * tmin + r.pos) - cube.start;
	pos /= cube.end;

	if(tmin == mi.x) {
		int isLeft = int(mi.x == startDir.x);
		hit.normal = vec3(isLeft * -2 + 1, 0, 0);
		hit.intersection = pos.yz + vec2(isLeft * 3 + 2, 0) + 2;
	}

	else if(tmin == mi.y) {
		int isDown = int(mi.y == startDir.y);
		hit.normal = vec3(0, isDown * -2 + 1, 0);
		hit.intersection = pos.xz + vec2(isDown + 3, 0) + 2;
	}

	else {
		int isBack = int(mi.z == startDir.z);
		hit.normal = vec3(0, 0, isBack * -2 + 1);
		hit.intersection = pos.xy + vec2(isBack * 5 + 1, 0) + 2;
	}

	//

	hit.hitT = tmin;
	return true;
}

bool sphereInPlane(const vec4 sphere, const vec4 plane) {
	return dot(plane.xyz, sphere.xyz) + plane.w <= sphere.w;
}