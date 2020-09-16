#pragma once
#include "types/scene_object_types.hpp"
#include "types/mat.hpp"
#include "gui/ui_value.hpp"

namespace igx::rt {

	struct Seed {

		f32 randomX, randomY;
		f32 cpuOffsetX, cpuOffsetY;

		u32 sampleCount, sampleOffset;
	};

	struct CPUCamera : public Camera {

		ui::Slider<f32, 0, 360_deg> pitch, yaw, roll;

		ui::Slider<f32, 0.01f, 10.f> speed = 5;
		ui::Slider<f32, 1, 179> leftFov = 70, rightFov = 70;

		Mat3x3f32 getRot() const;

		//Create a view matrix from the rotation and eye
		//If eyeOffset == 0, the camera will be put like a cyclops (in the center), used for relative movement
		//If eyeOffset == -1, camera will be on the left side
		//If eyeOffset == 1, camera will be on the right side
		//Could theoretically be expanded for 8-eyed creatures
		Mat4x4f32 getView(f32 eyeOffset) const;

		InflectParentedWithName(
			Camera, 
			{ "Speed", "Left FOV", "Right FOV", "Pitch (X)", "Yaw (Y)", "Roll (Z)" }, 
			speed, leftFov, rightFov,
			pitch, yaw, roll
		);

	};

}