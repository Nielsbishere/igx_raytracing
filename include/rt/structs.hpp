#pragma once
#include "types/scene_object_types.hpp"
#include "types/mat.hpp"

namespace igx::rt {

	struct Hit {

		Vec2u32 normal;
		Vec2u32 rayDir;

		Vec3f32 rayOrigin;
		f32 hitT;

		Vec2f32 uv;
		u32 object;
		u32 primitive;

	};

	struct Seed {

		f32 randomX, randomY;
		f32 cpuOffsetX, cpuOffsetY;

		u32 sampleCount;
	};

	struct CPUCamera : public Camera {

		Vec3f32 eyeDir = { 0, 0, -1 };

		f32 speed = 5;
		f32 fov = 70;

	};

}