
const float noHit = 3.4028235e38;
const float epsilon = 1e-5;

//Define very often used types

struct Ray { vec4 pos; vec4 dir; };

const uint RayFlag_Recurse = 1;		//Turned on if recursion for this ray is enabled
const uint RayFlag_CullBack = 2;	//Turned on if intersections with negative normals are ignored
const uint RayFlag_CullFront = 4;	//Turned on if intersections with positive normals are ignored

struct RayPayload {

	vec3 pos;				//Vector position
	uint rayFlag;			//Ray flags

	vec3 dir;				//Vector direction
	uint screenX;

	vec3 color;				//Color to be multiplied into result
	uint screenY;

};

struct Triangle {

	vec3 p0;
	uint object;

	vec3 p1;
	uint material;

	vec3 p2;
	uint padding;
};


//Ray intersections

bool rayIntersectSphere(const Ray r, const vec4 sphere, inout vec3 hit) {

	const vec4 dif = sphere - r.pos;
	const float t = dot(dif, r.dir);

	const vec4 Q = dif - t * r.dir;
	const float Q2 = dot(Q, Q);

	const bool outOfSphere = Q2 > sphere.w;
	const float hitT = t - sqrt(sphere.w - Q2);

	if(!outOfSphere && hitT >= 0 && hitT < hit.z) {
		hit.z = hitT;
		return true;
	}

	return false;
}

bool rayIntersectPlane(const Ray r, const vec4 plane, inout vec3 hit) {

	float hitT = -(dot(r.pos.xyz, plane.xyz) + plane.w) / dot(r.dir.xyz, plane.xyz);

	if(hitT >= 0 && hitT < hit.z) {
		hit.z = hitT;
		return true;
	}

	return false;
}

bool rayIntersectTri(const Ray r, const Triangle tri, inout vec3 hit) {

	const vec3 p1_p0 = tri.p1 - tri.p0;
	const vec3 p2_p0 = tri.p2 - tri.p0;
	
	const vec3 h = cross(r.dir.xyz, p2_p0);
	const float a = dot(p1_p0, h);
	
	if (abs(a) < epsilon) return false;
	
	const float f = 1 / a;
	const vec3 s = r.pos.xyz - tri.p0;
	const float u = f * dot(s, h);
	
	if (u < 0 || u > 1) return false; 
	
	const vec3 q = cross(s, p1_p0);
	const float v = f * dot(r.dir.xyz, q);
	
	if (v < 0 || u + v > 1) return false;
	
	const float t = f * dot(p2_p0, q);
	
	if (t <= epsilon || t >= 1 / epsilon || t >= hit.z) return false;

	hit = vec3(u, v, t);
	return true;
}

bool sphereInPlane(const vec4 sphere, const vec4 plane) {
	return dot(plane.xyz, sphere.xyz) + plane.w <= sphere.w;
}