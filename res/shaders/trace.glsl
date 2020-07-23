#include "primitive.glsl"
#include "rand_util.glsl"

//Buffers

const uint 
	DisplayType_Default = 0, 
	DisplayType_Accumulation = 1,
	DisplayType_Intersection_attributes = 2,
	DisplayType_Normals = 3,
	DisplayType_Albedo = 4,
	DisplayType_Light_buffer = 5,
	DisplayType_Reflection_buffer = 6,
	DisplayType_No_secondaries = 7,
	DisplayType_UI_Only = 8,
	DisplayType_Material = 9,
	DisplayType_Object = 10,
	DisplayType_Intersection_side = 11;

layout(binding=0, std140) uniform GPUData {

	vec3 eye;
	uint width;

	vec3 p0;
	uint height;

	vec3 p1;
	float randomX;

	vec3 p2;
	float randomY;

	vec3 skyboxColor;
	float exposure;

	vec2 invRes;
	float focalDistance;
	float aperature;

	uint sampleCount;
	uint triangleCount;
	uint sphereCount;
	uint cubeCount;

	uint planeCount;
	uint displayType;

};

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

//Intersections for colors

Hit traceGeometry(const Ray ray) {

	Hit hit;
	hit.hitT = noHit;
	hit.intersection = vec2(0);
	hit.material = hit.object = 0;
	hit.normal = vec3(0);

	//TODO: Only get normal of one object
	//TODO: Spheres and tris are upside down

	uint j = 0;

	#ifdef ALLOW_SPHERES

	for(int i = 0; i < sphereCount; ++i) {

		const vec4 sphere = spheres[i];

		if(rayIntersectSphere(ray, sphere, hit)) {
			hit.object = i;
			hit.material = i;
		}
	}

	j += sphereCount;

	#endif 

	#ifdef ALLOW_PLANES

	for(int i = 0; i < planeCount; ++i)
		if(rayIntersectPlane(ray, planes[i], hit)) {
			hit.material = i;
			hit.object = i + j;
		}

	j += planeCount;

	#endif

	#ifdef ALLOW_TRIANGLES

	for(int i = 0; i < triangleCount; ++i) {
		if(rayIntersectTri(ray, triangles[i], hit)) {
			hit.material = triangles[i].material;
			hit.object = i + j;
			hit.normal = normalize(cross(triangles[i].p1 - triangles[i].p0, triangles[i].p2 - triangles[i].p0));
		}
	}

	j += triangleCount;

	#endif

	#ifdef ALLOW_CUBES

	for(int i = 0; i < cubeCount; ++i) {
		if(rayIntersectCube(ray, cubes[i], hit)) {
			hit.material = cubes[i].material;
			hit.object = i + j;
		}
	}

	#endif

	return hit;
}

//Optimized intersections for shadows

bool traceOcclusion(const Ray ray, const float maxDist) {

	Hit hit;
	hit.hitT = noHit;

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

	for(int i = 0; i < cubes.length(); ++i)
		rayIntersectCube(ray, cubes[i], hit);

	#endif

	return hit.hitT < maxDist;
}

//Setting up primaries

Ray calculatePrimary(const uvec2 loc) {

	//Calculate center pixel, jittered for AA

	const vec2 centerPixel = (vec2(loc) + rand(loc + vec2(randomX, randomY))) * invRes;
	
	const vec3 right = p1 - p0;
	const vec3 up = p2 - p0;

	const vec3 pos = p0 + centerPixel.x * right + centerPixel.y * up;
	const vec3 dir = normalize(pos - eye);

	return Ray(pos, 0, dir, 0);
}