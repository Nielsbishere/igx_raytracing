#pragma once
#include "helpers/render_task.hpp"
#include "helpers/factory.hpp"
#include "gui/gui.hpp"
#include "gui/struct_inspector.hpp"
#include "../res/shaders/defines.glsl"

namespace igx::rt {

	struct CloudUniformBuffer {

		f32 Offset_x;
		ui::Slider<f32, -100, 100> Offset_y;
		f32 Offset_z;
		f32 Height_a;

		f32 Height_b;
		ui::Slider<f32, 0, 1> Absorption;
		ui::Slider<f32, 0, 1> Threshold;
		ui::Slider<f32, 0, 2> Multiplier;

		ui::Slider<f32, 0.01f, 16> Scale_xz;
		ui::Slider<f32, 0.01f, 16> Scale_y;
		ui::Slider<u32, 0, 256> Samples;
		ui::Slider<u32, 0, 64> Light_samples;

		Vec2u32 res;
		u32 directionalLights;

		Inflect(
			Samples, Light_samples,
			Absorption, Threshold, Multiplier,
			Height_a, Height_b,
			Scale_xz, Scale_y,
			Offset_x, Offset_y, Offset_z
		);

	};

	class CloudTask : public ParentTextureRenderTask {

		static constexpr Vec3u16 noiseResHQ = 128, noiseResLQ = 32;

		ui::GUI &gui;
		FactoryContainer &factory;

		GPUBufferRenderTask *primaries;

		PipelineRef shader;
		PipelineLayoutRef shaderLayout;

		DescriptorsRef descriptors;

		GPUBufferRef uniformBuffer;
		CloudUniformBuffer *cloudData;

		SamplerRef linearSampler;
		TextureRef noiseOutputHQ, noiseOutputLQ;

		ui::Slider<f32, -1, 1> windDirectionX = -0.1f, windDirectionZ = -1.f;
		ui::Slider<f32, 0.1f, 10> windSpeed = 2.f;

	public:

		CloudTask(GPUBufferRenderTask *primaries, FactoryContainer &factory, ui::GUI &gui);

		void prepareCommandList(CommandList *cl) override;

		void update(f64 dt) override;
		void resize(const Vec2u32 &size) override;

		void switchToScene(SceneGraph *sceneGraph) override;

		/*
		InflectWithName(
			{ 
				"", 
				"Wind direction x", "Wind direction z", "Wind speed" 
				"Noise"
			}, 
			*cloudData, 
			windDirectionX, windDirectionZ, windSpeed,
			*tasks[0]
		);
		*/
	};

}