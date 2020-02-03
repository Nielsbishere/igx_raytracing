#include "primitive.glsl"

layout(binding=0, std140) readonly buffer Spheres {
	vec4 spheres[];		//pos, rad^2
};

layout(binding=1, std140) readonly buffer Planes {
	vec4 planes[];		//dir, offset
};

layout(binding=2, std140) readonly buffer Triangles {
	Triangle triangles[];
};


//TODO:
//const uint RayFlag_Recurse = 1;		//Turned on if recursion for this ray is enabled
//const uint RayFlag_CullBack = 2;	//Turned on if intersections with negative normals are ignored
//const uint RayFlag_CullFront = 4;	//Turned on if intersections with positive normals are ignored

struct RayPayload {

	vec2 dir;				//Vector direction in spherical coords
	uint screenCoord;
	uint flagLayer;

	vec3 color;				//Color to be multiplied into result
	float maxDist;

};

void traceGeometry(const Ray ray, inout vec3 hit, inout vec3 normal, inout uint material) {

	//TODO: Spheres and tris are upside down

	#ifdef ALLOW_SPHERES

	for(int i = 0; i < spheres.length(); ++i) {
		if(rayIntersectSphere(ray, spheres[i], hit)) {
			material = i;
			normal = normalize(hit.z * ray.dir.xyz + ray.pos.xyz - spheres[i].xyz);
		}
	}

	#endif 

	#ifdef ALLOW_PLANES

	for(int i = 0; i < planes.length(); ++i) {
		if(rayIntersectPlane(ray, planes[i], hit)) {
			material = i;
			normal = -planes[i].xyz;
		}
	}

	#endif

	#ifdef ALLOW_TRIANGLES

	for(int i = 0; i < triangles.length(); ++i) {
		if(rayIntersectTri(ray, triangles[i], hit)) {
			material = triangles[i].material;
			normal = -cross(triangles[i].p1 - triangles[i].p0, triangles[i].p2 - triangles[i].p0);	//Flat shading for now TODO: Normals
		}
	}

	#endif

}

bool traceOcclusion(const Ray ray, const float maxDist) {

	vec3 hit = vec3(0, 0, noHit);

	#ifdef ALLOW_SPHERES

	for(int i = 0; i < spheres.length(); ++i)
		rayIntersectSphere(ray, spheres[i], hit);

	#endif

	#ifdef ALLOW_PLANES

	for(int i = 0; i < planes.length(); ++i)
		rayIntersectPlane(ray, planes[i], hit);

	#endif

	#ifdef ALLOW_TRIANGLES

	for(int i = 0; i < triangles.length(); ++i)
		rayIntersectTri(ray, triangles[i], hit);

	#endif

	return hit.z < maxDist;
}