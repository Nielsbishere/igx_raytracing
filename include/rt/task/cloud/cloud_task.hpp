#pragma once
#include "rt/task/raygen_task.hpp"
#include "gui/gui.hpp"
#include "gui/struct_inspector.hpp"
#include "../res/shaders/defines.glsl"

namespace igx::rt {

	//Noise customizers

	struct CloudBuffer {

		f32 Offset_x{};
		ui::Slider<f32, -100, 100> Offset_y{};
		f32 Offset_z{};
		f32 Height_a = 20;

		f32 Height_b = 61;
		ui::Slider<f32, 0, 1> Absorption = 0.95f;
		ui::Slider<f32, 0, 1> Threshold = 0.40f;
		ui::Slider<f32, 0, 2> Multiplier = 0.358f;

		ui::Slider<f32, 0.01f, 16> Scale_xz = 5.2f;
		ui::Slider<f32, 0.01f, 16> Scale_y = 3.9f;
		ui::Slider<u32, 0, 256> Samples = 96;
		ui::Slider<u32, 0, 64> Light_samples = 32;

		u32 directionalLights;

		Inflect(
			Samples, Light_samples,
			Absorption, Threshold, Multiplier,
			Height_a, Height_b,
			Scale_xz, Scale_y,
			Offset_x, Offset_y, Offset_z
		);

	};

	struct CPUCloudBuffer : public CloudBuffer {

		ui::Slider<f32, -1, 1> Wind_direction_x = -0.43f, Wind_direction_z = -1.f;
		ui::Slider<f32, 0.1f, 10> Wind_speed = 2.f;

		InflectParented(CloudBuffer, Wind_direction_x, Wind_direction_z, Wind_speed);

	};

	struct NoiseUniformData {

		Vec3f32 Offset_0;
		ui::Slider<f32, 0.1f, 10.f> Persistence = 2.64f;

		Vec3f32 Offset_1;
		bool Is_inverted{}; u8 pad[3]{};

		Vec3f32 Offset_2;
		ui::Slider<u32, 1, 128> Points_x0 = 47;

		ui::Slider<u32, 1, 128> Points_x1 = 53, Points_y1 = 35, Points_z1 = 45;
		ui::Slider<u32, 1, 128> Points_y0 = 61;

		ui::Slider<u32, 1, 128> Points_x2 = 18, Points_y2 = 53, Points_z2 = 22;
		ui::Slider<u32, 1, 128> Points_z0 = 46;

		Inflect(
			Persistence, Is_inverted,
			Points_x0, Points_y0, Points_z0, Offset_0,
			Points_x1, Points_y1, Points_z1, Offset_1,
			Points_x2, Points_y2, Points_z2, Offset_2
		);

	};

	//

	class CloudSubtask;

	class CloudTask : public RenderTask {

		static constexpr Vec3u16 noiseResHQ = 128, noiseResLQ = 32;

		ui::GUI &gui;
		FactoryContainer &factory;

		RenderTasks tasks;

		RaygenTask *primaries;

		GPUBufferRef uniformBuffer, noiseUniformData;
		ui::StructInspector<CPUCloudBuffer> cloudBuffer;
		ui::StructInspector<NoiseUniformData> noiseUniforms;

		DescriptorsRef cloudDescriptors;
		PipelineLayoutRef cloudLayout;
		SamplerRef linearSampler;

		TextureRef noiseOutputHQ, noiseOutputLQ;

		CloudSubtask *subtasks[2];

	public:

		CloudTask(RaygenTask *primaries, FactoryContainer &factory, ui::GUI &gui, const DescriptorsRef &camera);
		~CloudTask();

		void resize(const Vec2u32 &size) override;

		bool needsCommandUpdate() const;
		void prepareCommandList(CommandList *cl) override;

		void update(f64 dt) override;

		void switchToScene(SceneGraph *sceneGraph) override;

		Texture *getOutput(bool isShadow = false) const;
	};

}