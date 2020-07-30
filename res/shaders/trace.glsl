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
	DisplayType_Shadows_no_reflection = 11,
	DisplayType_Dispatch_groups = 12,
	DisplayType_Reflection_albedo = 13,
	DisplayType_Reflection_normals = 14,
	DisplayType_Reflection_material = 15,
	DisplayType_Reflection_object = 16,
	DisplayType_Reflection_intersection_attributes = 17,
	DisplayType_Reflection_of_reflection = 18,
	DisplayType_Clouds = 19;

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
	float ipd;

	vec3 p2;
	uint projectionType;

	vec3 skyboxColor;
	float exposure;

	vec2 invRes;
	float focalDistance;
	float aperature;

	uint disableUI;
	uint triangleCount;
	uint sphereCount;
	uint cubeCount;

	uint planeCount;
	uint displayType;
	bool useSkybox;

};

layout(binding=14, std430) buffer InitData {
	float randomX, randomY;
	float cpuOffsetX, cpuOffsetY;
	uint sampleCount;
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

Hit traceGeometry(const Ray ray, uint prevHit) {

	Hit hit;
	hit.hitT = noHit;
	hit.intersection = vec2(0);
	hit.material = hit.object = 0;
	hit.normal = vec3(0);

	//TODO: Only get normal and uv of one object

	uint j = 0;

	#ifdef ALLOW_SPHERES

	for(int i = 0; i < sphereCount; ++i)
		if(rayIntersectSphere(ray, spheres[i], hit, i, prevHit)) {
			hit.object = i;
			hit.material = i;
		}

	j += sphereCount;

	#endif 

	#ifdef ALLOW_PLANES

	for(int i = 0; i < planeCount; ++i)
		if(rayIntersectPlane(ray, planes[i], hit, i + j, prevHit)) {
			hit.material = i;
			hit.object = i + j;
		}

	j += planeCount;

	#endif

	#ifdef ALLOW_TRIANGLES

	for(int i = 0; i < triangleCount; ++i) {
		if(rayIntersectTri(ray, triangles[i], hit, i + j, prevHit)) {
			hit.material = triangles[i].material;
			hit.object = i + j;
		}
	}

	j += triangleCount;

	#endif

	#ifdef ALLOW_CUBES

	for(int i = 0; i < cubeCount; ++i) {
		if(rayIntersectCube(ray, cubes[i], hit, i + j, prevHit)) {
			hit.material = cubes[i].material;
			hit.object = i + j;
		}
	}

	#endif

	return hit;
}

//Optimized intersections for shadows

bool traceOcclusion(const Ray ray, const float maxDist, uint prevHit) {

	Hit hit;
	hit.hitT = noHit;

	uint j = 0;

	#ifdef ALLOW_SPHERES

	for(int i = 0; i < sphereCount; ++i)
		rayIntersectSphere(ray, spheres[i], hit, i, prevHit);

	j += sphereCount;

	#endif

	#ifdef ALLOW_PLANES

	for(int i = 0; i < planeCount; ++i)
		rayIntersectPlane(ray, planes[i], hit, j + i, prevHit);

	j += planeCount;

	#endif

	#ifdef ALLOW_TRIANGLES

	for(int i = 0; i < triangleCount; ++i)
		rayIntersectTri(ray, triangles[i], hit, j + i, prevHit);

	j += triangleCount;

	#endif

	#ifdef ALLOW_CUBES

	for(int i = 0; i < cubeCount; ++i)
		rayIntersectCube(ray, cubes[i], hit, j + i, prevHit);

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

const vec2 invAtan = vec2(0.1591, 0.3183);

vec2 sampleEquirect(const vec3 n) {
	const vec2 uv = vec2(atan(n.x, n.z), asin(n.y * -1)) * invAtan;
	return uv + 0.5;
}

layout(binding=1) uniform sampler2D skybox;

vec3 sampleSkybox(const vec3 dir) {

	if(!useSkybox)
		return skyboxColor;

	return texture(skybox, sampleEquirect(dir)).rgb;
}