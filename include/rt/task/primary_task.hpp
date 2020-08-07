#pragma once
#include "helpers/render_task.hpp"
#include "helpers/factory.hpp"

namespace igx::rt {

	class PrimaryTask : public GPUBufferRenderTask {

		FactoryContainer &factory;

		SceneGraph *sceneGraph;

		PipelineRef shader;
		PipelineLayoutRef shaderLayout;

		DescriptorsRef descriptors, cameraDescriptor;
		GPUBufferRef seedBuffer;

	public:

		PrimaryTask(
			FactoryContainer &factory,
			const GPUBufferRef &seedBuffer,
			const DescriptorsRef &cameraDescriptor
		);

		void prepareCommandList(CommandList *cl) override;

		void update(f64 dt) override;
		void resize(const Vec2u32 &size) override;

		void switchToScene(SceneGraph *sceneGraph) override;
	};

}