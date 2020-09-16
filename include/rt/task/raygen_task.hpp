#pragma once
#include "helpers/render_task.hpp"
#include "helpers/factory.hpp"

namespace igx::rt {

	class RaygenTask : public TextureRenderTask {

		FactoryContainer &factory;

		SceneGraph *sceneGraph;

		PipelineRef shader;
		PipelineLayoutRef shaderLayout;

		DescriptorsRef descriptors, cameraDescriptor;
		GPUBufferRef seedBuffer;

	public:

		RaygenTask(
			FactoryContainer &factory,
			const GPUBufferRef &seedBuffer,
			const DescriptorsRef &cameraDescriptor
		);

		void prepareCommandList(CommandList *cl) override;

		void update(f64) override {}
		void resize(const Vec2u32 &size) override;

		void switchToScene(SceneGraph *sceneGraph) override;
	};

}