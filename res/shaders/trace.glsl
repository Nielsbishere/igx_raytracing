#ifndef TRACE

#include "rand_util.glsl"
#include "camera.glsl"
#include "scene.glsl"

//Intersections for colors

Hit traceGeometry(const Ray ray, uint prevHit) {

	Hit hit;

	hit.rayOrigin = ray.pos;
	hit.rayDir = ray.dir;
	hit.normal = 0;

	hit.hitT = noHit;
	hit.uv = vec2(0);
	hit.object = hit.primitive = 0;

	//TODO: Only get normal and uv of one object

	uint j = 0;

	#ifdef ALLOW_TRIANGLES
		for(int i = 0; i < sceneInfo.triangleCount; ++i, ++j)
			if(rayIntersectTri(ray, triangles[i], hit, j, prevHit))
				hit.object = j;
	#endif

	#ifdef ALLOW_SPHERES
		for(uint i = 0; i < sceneInfo.sphereCount; ++i, ++j)
			if(rayIntersectSphere(ray, spheres[i], hit, j, prevHit))
				hit.object = j;
	#endif 

	#ifdef ALLOW_CUBES
		for(int i = 0; i < sceneInfo.cubeCount; ++i, ++j)
			if(rayIntersectCube(ray, cubes[i], hit, j, prevHit))
				hit.object = j;
	#endif

	#ifdef ALLOW_PLANES
		for(uint i = 0; i < sceneInfo.planeCount; ++i, ++j)
			if(rayIntersectPlane(ray, planes[i], hit, j, prevHit))
				hit.object = j;
	#endif

	return hit;
}

//Optimized intersections for shadows

bool traceOcclusion(const Ray ray, const float maxDist, uint prevHit) {

	Hit hit;
	hit.hitT = noHit;

	uint j;

	#ifdef ALLOW_TRIANGLES
		for(int i = 0; i < sceneInfo.triangleCount; ++i, ++j)
			rayIntersectTri(ray, triangles[i], hit, j, prevHit);
	#endif

	#ifdef ALLOW_SPHERES
		for(uint i = 0; i < sceneInfo.sphereCount; ++i, ++j)
			rayIntersectSphere(ray, spheres[i], hit, j, prevHit);
	#endif

	#ifdef ALLOW_CUBES
		for(int i = 0; i < sceneInfo.cubeCount; ++i, ++j)
			rayIntersectCube(ray, cubes[i], hit, j, prevHit);
	#endif

	#ifdef ALLOW_PLANES
		for(uint i = 0; i < sceneInfo.planeCount; ++i, ++j)
			rayIntersectPlane(ray, planes[i], hit, j, prevHit);
	#endif

	return hit.hitT < maxDist;
}

#endif