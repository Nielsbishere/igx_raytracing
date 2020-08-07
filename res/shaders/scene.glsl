#ifndef SCENE
#define SCENE

#include "camera.glsl"

struct SceneInfo {

	uint triangleCount;
	uint lightCount;
	uint materialCount;
	uint cubeCount;

	uint sphereCount;
	uint planeCount;
	uint directionalLightCount;
	uint spotLightCount;

	uint pointLightCount;

};

layout(binding=1, std140) uniform SceneData {
	SceneInfo sceneInfo;
};

layout(binding=0, std430) readonly buffer Triangles {
	Triangle triangles[];
};

layout(binding=1, std140) readonly buffer Spheres {
	vec4 spheres[];		//pos, rad^2
};

layout(binding=2, std430) readonly buffer Cubes {
	Cube cubes[];
};

layout(binding=3, std140) readonly buffer Planes {
	vec4 planes[];		//dir, offset
};

layout(binding=4, std430) readonly buffer Lights {
	Light lights[];
};

layout(binding=5, std140) readonly buffer Materials {
	Material materials[];
};

layout(binding=0) uniform sampler2D skybox;

const vec2 invAtan = vec2(0.1591, 0.3183);

vec2 sampleEquirect(const vec3 n) {
	const vec2 uv = vec2(atan(n.x, n.z), asin(n.y * -1)) * invAtan;
	return uv + 0.5;
}

vec3 sampleSkybox(const vec3 dir) {

	if(any(lessThanEqual(textureSize(skybox, 0), ivec2(0))))
		return camera.skyboxColor;

	return texture(skybox, sampleEquirect(dir)).rgb;
}

#endif