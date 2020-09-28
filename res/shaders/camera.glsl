#ifndef CAMERA
#define CAMERA
#include "primitive.glsl"
#include "rand_util.glsl"

const uint 
	ProjectionType_Default = 0, 
	ProjectionType_Omnidirectional = 1,
	ProjectionType_Stereoscopic_omnidirectional_TB = 2,
	ProjectionType_Stereoscopic_TB = 3,
	ProjectionType_Stereoscopic_omnidirectional_LR = 4,
	ProjectionType_Stereoscopic_LR = 5;

struct Camera {

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
	
	vec3 p3;
	float focalDistance;

	vec3 p4;
	float aperature;

	vec3 p5;
	uint flags;

	vec2 invRes;
	uvec2 tiles;
};

const uint CameraType_USE_UI = 1 << 0;
const uint CameraType_USE_SUPERSAMPLING = 1 << 1;

layout(binding=0, std140) uniform CameraData {
	Camera camera;
};

//Setting up primaries

Ray calculateOmni(const vec2 centerPixel, bool isLeft) {

	const vec2 spherical = vec2(centerPixel.x - 0.5, 0.5 - centerPixel.y) * vec2(2 * pi, pi);

	const vec2 sins = sin(spherical), coss = cos(spherical);

	//TODO: Should be facing the camera's eyeDir

	//Convert ipd to ipdInMeters / 2, to get two eyes sitting apart with ipd

	const vec3 pos = 
		camera.eye + vec3(coss.x, 0, sins.x) * 
		(camera.ipd * 5e-4) * (isLeft ? -1 : 1);	

	const vec3 dir = vec3(sins.x * coss.y, sins.y, -coss.x * coss.y);

	return Ray(pos, dir);
}

Ray calculateScreen(const vec2 centerPixel, bool isRight) {

	const vec3 p0 = isRight ? camera.p3 : camera.p0;
	const vec3 p1 = isRight ? camera.p4 : camera.p1;
	const vec3 p2 = isRight ? camera.p5 : camera.p2;

	const vec3 right = p1 - p0;
	const vec3 up = p2 - p0;

	const vec3 pos = p0 + centerPixel.x * right + centerPixel.y * up;
	const vec3 dir = normalize(pos - camera.eye);

	//TODO: What happens if you shoot from camera.eye instead?

	return Ray(camera.eye, dir);
}

Ray calculateOmniStereoTB(const vec2 centerPixel) {
	return calculateOmni(vec2(centerPixel.x, fract(centerPixel.y * 2)), centerPixel.y < 0.5);
}

Ray calculateOmniStereoLR(const vec2 centerPixel) {
	return calculateOmni(vec2(fract(centerPixel.x * 2), centerPixel.y), centerPixel.x < 0.5);
}

Ray calculateStereoTB(vec2 centerPixel) {

	if(centerPixel.y < 0.5)
		return calculateScreen(vec2(centerPixel.x, centerPixel.y * 2), false);

	return calculateScreen(vec2(centerPixel.x, centerPixel.y * 2 - 1), true);
}

Ray calculateStereoLR(vec2 centerPixel) {

	if(centerPixel.x < 0.5)
		return calculateScreen(vec2(centerPixel.x * 2, centerPixel.y), false);

	return calculateScreen(vec2(centerPixel.x * 2 - 1, centerPixel.y), true);
}

Ray calculatePrimary(const uvec2 loc, const vec2 randLoc) {

	vec2 centerPixel = (vec2(loc) + rand(loc + randLoc)) * camera.invRes;
	centerPixel.y = 1 - centerPixel.y;

	switch(camera.projectionType) {

		case ProjectionType_Omnidirectional: 
			return calculateOmni(centerPixel, false);

		case ProjectionType_Stereoscopic_omnidirectional_TB: 
			return calculateOmniStereoTB(centerPixel);

		case ProjectionType_Stereoscopic_omnidirectional_LR: 
			return calculateOmniStereoLR(centerPixel);

		case ProjectionType_Stereoscopic_TB: 
			return calculateStereoTB(centerPixel);

		case ProjectionType_Stereoscopic_LR: 
			return calculateStereoLR(centerPixel);

		default:
			return calculateScreen(centerPixel, false);
	}
}

uint calculateTiled(uvec2 loc) {

	//Calculate current index in tilemap

	uvec2 tile = loc >> THREADS_XY_SHIFT;
	uint tile1D = (tile.x + tile.y * camera.tiles.x) << (2 * THREADS_XY_SHIFT);

	//Calculate current index in tile

	uvec2 inTile = loc & THREADS_XY_MASK;
	uint inTile1D = inTile.x | (inTile.y << THREADS_XY_SHIFT);

	//Calculate global offset

	return tile1D | inTile1D;
}

vec4 calculatePlane(vec3 p0, vec3 p1, vec3 p2) {

	vec3 p1_p0 = normalize(p0 - p1);
	vec3 p2_p0 = normalize(p2 - p0);

	vec3 n = cross(p1_p0, p2_p0);
	vec3 np = n * p0;

	float d = -np.x - np.y - np.z;
	return vec4(n, d);
}

Frustum calculateFrustum(vec2 begin, vec2 end, vec3 n, float minHitT, float maxHitT, bool isRight) {

	Frustum f;

	f.planes[0] = vec4(n, minHitT);			//n (facing to -z in cam space)
	f.planes[1] = vec4(-n, -maxHitT);		//f (facing +z in cam space)

	return f;
}

#endif