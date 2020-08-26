#ifndef CAMERA
#define CAMERA
#include "primitive.glsl"

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
	uint pad0;

	vec2 invRes;
	uvec2 tiles;
};

layout(binding=0, std140) uniform CameraData {
	Camera camera;
};

//Setting up primaries

const float pi = 3.1415927410125732421875;

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

Ray calculateScreen(const vec2 centerPixel) {

	const vec3 right = camera.p1 - camera.p0;
	const vec3 up = camera.p2 - camera.p0;

	const vec3 pos = camera.p0 + centerPixel.x * right + centerPixel.y * up;
	const vec3 dir = normalize(pos - camera.eye);

	return Ray(pos, dir);
}

Ray calculateScreenRight(const vec2 centerPixel) {

	const vec3 right = camera.p4 - camera.p3;
	const vec3 up = camera.p5 - camera.p3;

	const vec3 pos = camera.p3 + centerPixel.x * right + centerPixel.y * up;
	const vec3 dir = normalize(pos - camera.eye);

	return Ray(pos, dir);
}

Ray calculateOmniStereoTB(const vec2 centerPixel) {
	return calculateOmni(vec2(centerPixel.x, fract(centerPixel.y * 2)), centerPixel.y < 0.5);
}

Ray calculateOmniStereoLR(const vec2 centerPixel) {
	return calculateOmni(vec2(fract(centerPixel.x * 2), centerPixel.y), centerPixel.x < 0.5);
}

Ray calculateStereoTB(vec2 centerPixel) {

	if(centerPixel.y < 0.5)
		return calculateScreen(vec2(centerPixel.x, centerPixel.y * 2));

	return calculateScreenRight(vec2(centerPixel.x, centerPixel.y * 2 - 1));
}

Ray calculateStereoLR(vec2 centerPixel) {

	if(centerPixel.x < 0.5)
		return calculateScreen(vec2(centerPixel.x * 2, centerPixel.y));

	return calculateScreenRight(vec2(centerPixel.x * 2 - 1, centerPixel.y));
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
			return calculateScreen(centerPixel);
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

#endif