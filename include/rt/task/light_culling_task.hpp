#pragma once
#include "helpers/render_task.hpp"
#include "helpers/factory.hpp"

namespace igx::rt {

	class RaygenTask;

	class LightCullingTask : public TextureRenderTask {

		FactoryContainer &factory;

		SceneGraph *sceneGraph;

		SamplerRef nearestSampler;
		PipelineRef pipeline;
		PipelineLayoutRef layout;

		DescriptorsRef descriptors, cameraDescriptor;

		RaygenTask *raygenTask;

	public:

		LightCullingTask(
			RaygenTask *raygenTask,
			FactoryContainer &factory,
			const DescriptorsRef &cameraDescriptor
		);

		void prepareCommandList(CommandList *cl) override;

		void update(f64 dt) override;
		void resize(const Vec2u32 &size) override;

		void switchToScene(SceneGraph *sceneGraph) override;
	};

}