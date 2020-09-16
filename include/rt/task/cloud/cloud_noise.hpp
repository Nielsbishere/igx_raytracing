#pragma once
#include "rt/task/raygen_task.hpp"
#include "gui/gui.hpp"
#include "gui/struct_inspector.hpp"
#include "../res/shaders/defines.glsl"

namespace igx::rt {

	class CloudNoiseTask : public RenderTask {

		FactoryContainer &factory;

		TextureRef output;

		GPUBufferRef data, targetRes;

		DescriptorsRef descriptors;
		PipelineLayoutRef layout;
		PipelineRef shader;

	public:

		CloudNoiseTask(FactoryContainer &factory, const String &name, const GPUBufferRef &data, const TextureRef &output);

		void prepareCommandList(CommandList *cl) override;

		void resize(const Vec2u32&) override {}

		void update(f64) override {}
		void switchToScene(SceneGraph*) override {}
	};

}