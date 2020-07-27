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

const uint 
	ProjectionType_Default = 0, 
	ProjectionType_Omnidirectional = 1,
	ProjectionType_Equirect = 2,
	ProjectionType_Stereoscopic_omnidirectional_TB = 3,
	ProjectionType_Stereoscopic_TB = 4,
	ProjectionType_Stereoscopic_omnidirectional_LR = 5,
	ProjectionType_Stereoscopic_LR = 6;

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
	uint projectionType;
	float ipd;

	uint disableUI;

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

	//TODO: Only get normal and uv of one object

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

const float pi = 3.1415927410125732421875;

Ray calculateOmni(const vec2 centerPixel, bool isLeft) {

	const vec2 spherical = vec2(centerPixel.x - 0.5, 0.5 - centerPixel.y) * vec2(2 * pi, pi);

	const vec2 sins = sin(spherical), coss = cos(spherical);

	//TODO: Should be facing the camera's eyeDir

	//Convert ipd to ipdInMeters / 2, to get two eyes sitting apart with ipd

	const vec3 pos = eye + vec3(coss.x, 0, sins.x) * (ipd * 5e-4) * (isLeft ? - 1 : 1);	
	const vec3 dir = vec3(sins.x * coss.y, sins.y, -coss.x * coss.y);

	return Ray(pos, 0, dir, 0);
}

Ray calculateEquirect(const vec2 centerPixel) {

	//TODO: Should be facing the camera's eyeDir

	const vec2 spherical = centerPixel * vec2(2 * pi, pi);

	const vec2 sins = sin(spherical), coss = cos(spherical);

	const vec3 dir = vec3(coss.y * sins.x, sins.y * sins.x, coss.x);

	return Ray(eye, 0, dir, 0);
}

Ray calculateScreen(const vec2 centerPixel) {

	const vec3 right = p1 - p0;
	const vec3 up = p2 - p0;

	const vec3 pos = p0 + centerPixel.x * right + centerPixel.y * up;
	const vec3 dir = normalize(pos - eye);

	return Ray(pos, 0, dir, 0);
}

Ray calculateOmniStereoTB(const vec2 centerPixel) {
	return calculateOmni(vec2(centerPixel.x, fract(centerPixel.y * 2)), centerPixel.y < 0.5);
}

Ray calculateOmniStereoLR(const vec2 centerPixel) {
	return calculateOmni(vec2(fract(centerPixel.x * 2), centerPixel.y), centerPixel.x < 0.5);
}

Ray calculatePrimary(const uvec2 loc) {

	vec2 centerPixel = (vec2(loc) + rand(loc + vec2(randomX, randomY))) * invRes;
	centerPixel.y = 1 - centerPixel.y;

	//Don't use a switch case or if statement, or nvidia drivers will give unexplained behavior

	return 
		projectionType == ProjectionType_Default ? calculateScreen(centerPixel) : 
		(
			projectionType == ProjectionType_Omnidirectional ? calculateOmni(centerPixel, false) : 
			(
				projectionType == ProjectionType_Stereoscopic_omnidirectional_TB ? calculateOmniStereoTB(centerPixel) :
				(
					projectionType == ProjectionType_Stereoscopic_omnidirectional_LR ? calculateOmniStereoLR(centerPixel) :
					calculateEquirect(centerPixel)
				)
			)
		);
}