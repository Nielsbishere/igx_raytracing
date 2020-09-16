#pragma once
#include "rt/task/raygen_task.hpp"
#include "gui/gui.hpp"
#include "gui/struct_inspector.hpp"
#include "../res/shaders/defines.glsl"

namespace igx::rt {

	class CloudSubtask : public TextureRenderTask {

		FactoryContainer &factory;

		RaygenTask *primaries;

		GPUBufferRef uniformBuffer;
		TextureRef noiseOutputHQ, noiseOutputLQ;

		DescriptorsRef cloudDescriptors, outputDescriptor, cameraDescriptor;

		PipelineRef shader;
		PipelineLayoutRef shaderLayout;
		SamplerRef linearSampler;

	public:

		CloudSubtask(
			FactoryContainer &factory, const String &name, bool isShadowPass, RaygenTask *primaries, 
			List<RegisterLayout> layouts, const DescriptorsRef &cloudDescriptors,
			const GPUBufferRef &uniformBuffer, const TextureRef &noiseOutputHQ, const TextureRef &noiseOutputLQ,
			const DescriptorsRef &cameraDescriptor
		);

		void prepareCommandList(CommandList *cl) override;
		void resize(const Vec2u32 &size) override;
		void switchToScene(SceneGraph*) override {}
		void update(f64) override {}
	};

}