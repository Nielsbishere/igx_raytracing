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

layout(binding=10, std140) readonly buffer Cubes {
	Cube cubes[];
};

struct RayPayload {

	vec3 dir;				//Vector direction in spherical coords
	uint screenCoord;

	uvec2 color;			//Color packed into 2 halfs, alpha used for layer
	float maxDist;

};

void traceGeometry(const Ray ray, inout vec3 hit, inout vec3 normal, inout uint material) {

	//TODO: Spheres and tris are upside down

	#ifdef ALLOW_SPHERES

	for(int i = 0; i < spheres.length(); ++i) {

		const vec4 sphere = spheres[i];

		if(rayIntersectSphere(ray, sphere, hit)) {
			material = i;
			normal = normalize(sphere.xyz - (hit.z * ray.dir + ray.pos));
		}
	}

	#endif 

	#ifdef ALLOW_PLANES

	for(int i = 0; i < planes.length(); ++i) {
		if(rayIntersectPlane(ray, planes[i], hit)) {
			material = i;
			normal = normalize(planes[i].xyz);
		}
	}

	#endif

	#ifdef ALLOW_TRIANGLES

	//Flat shading for now TODO: Normals

	for(int i = 0; i < triangles.length(); ++i) {
		if(rayIntersectTri(ray, triangles[i], hit)) {
			material = triangles[i].material;
			normal = normalize(cross(triangles[i].p1 - triangles[i].p0, triangles[i].p2 - triangles[i].p0));
		}
	}

	#endif

	#ifdef ALLOW_CUBES

	//TODO: Normals

	for(int i = 0; i < cubes.length(); ++i) {
		if(rayIntersectCube(ray, cubes[i], hit)) {
			material = cubes[i].material;
			normal = normalize((cubes[i].end + cubes[i].start) * 0.5 - (hit.z * ray.dir + ray.pos));
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

	#ifdef ALLOW_CUBES

	//TODO: Crashes NVIDIA!

	//for(int i = 0; i < cubes.length(); ++i)
	//	rayIntersectCube(ray, cubes[i], hit);

	#endif

	return hit.z < maxDist;
}