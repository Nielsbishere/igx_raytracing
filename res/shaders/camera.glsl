#ifndef CAMERA
#define CAMERA
#include "primitive.glsl"

const uint 
	ProjectionType_Default = 0, 
	ProjectionType_Omnidirectional = 1,
	ProjectionType_Equirect = 2,
	ProjectionType_Stereoscopic_omnidirectional_TB = 3,
	ProjectionType_Stereoscopic_TB = 4,
	ProjectionType_Stereoscopic_omnidirectional_LR = 5,
	ProjectionType_Stereoscopic_LR = 6;

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

	vec2 invRes;
	float focalDistance;
	float aperature;
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
		(camera.ipd * 5e-4) * (isLeft ? - 1 : 1);	

	const vec3 dir = vec3(sins.x * coss.y, sins.y, -coss.x * coss.y);

	return Ray(pos, dir);
}

Ray calculateEquirect(const vec2 centerPixel) {

	//TODO: Should be facing the camera's eyeDir

	const vec2 spherical = centerPixel * vec2(2 * pi, pi);

	const vec2 sins = sin(spherical), coss = cos(spherical);

	const vec3 dir = vec3(coss.y * sins.x, sins.y * sins.x, coss.x);

	return Ray(camera.eye, dir);
}

Ray calculateScreen(const vec2 centerPixel) {

	const vec3 right = camera.p1 - camera.p0;
	const vec3 up = camera.p2 - camera.p0;

	const vec3 pos = camera.p0 + centerPixel.x * right + centerPixel.y * up;
	const vec3 dir = normalize(pos - camera.eye);

	return Ray(pos, dir);
}

Ray calculateOmniStereoTB(const vec2 centerPixel) {
	return calculateOmni(vec2(centerPixel.x, fract(centerPixel.y * 2)), centerPixel.y < 0.5);
}

Ray calculateOmniStereoLR(const vec2 centerPixel) {
	return calculateOmni(vec2(fract(centerPixel.x * 2), centerPixel.y), centerPixel.x < 0.5);
}

Ray calculatePrimary(const uvec2 loc, const vec2 randLoc) {

	vec2 centerPixel = (vec2(loc) + rand(loc + randLoc)) * camera.invRes;
	centerPixel.y = 1 - centerPixel.y;

	switch(camera.projectionType) {

		case ProjectionType_Omnidirectional: 
			return calculateOmni(centerPixel, false);

		case ProjectionType_Equirect:
			return calculateEquirect(centerPixel);

		case ProjectionType_Stereoscopic_omnidirectional_TB: 
			return calculateOmniStereoTB(centerPixel);

		case ProjectionType_Stereoscopic_omnidirectional_LR: 
			return calculateOmniStereoLR(centerPixel);

		default:
			return calculateScreen(centerPixel);
	}
}


#endif