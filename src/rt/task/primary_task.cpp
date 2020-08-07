#include "rt/task/primary_task.hpp"
#include "rt/enums.hpp"
#include "rt/structs.hpp"
#include "helpers/scene_graph.hpp"
#include "../res/shaders/defines.glsl"

namespace igx::rt {

	PrimaryTask::PrimaryTask(
		FactoryContainer &factory, 
		const GPUBufferRef &seedBuffer,
		const DescriptorsRef &cameraDescriptor
	) :
		GPUBufferRenderTask(
			factory.getGraphics(), GPUBuffer::Info(
				sizeof(Hit), GPUBufferType::STRUCTURED, GPUMemoryUsage::GPU_WRITE_ONLY
			), NAME("Primary task")
		),

		factory(factory),
		seedBuffer(seedBuffer),
		cameraDescriptor(cameraDescriptor)
	{
		//Setup shader

		auto raytracingLayout = SceneGraph::getLayout();

		raytracingLayout.push_back(RegisterLayout(
			NAME("HitBuffer"), 9, GPUBufferType::STRUCTURED, 6, 2,
			ShaderAccess::COMPUTE, sizeof(Hit), true
		));

		raytracingLayout.push_back(RegisterLayout(
			NAME("SeedBuffer"), 10, GPUBufferType::STORAGE, 7, 2,
			ShaderAccess::COMPUTE, sizeof(Seed)
		));

		shaderLayout = factory.get(
			NAME("Primary shader layout"),
			PipelineLayout::Info(raytracingLayout)
		);

		descriptors = {
			g, NAME("Primary task descriptors"),
			Descriptors::Info(
				shaderLayout, 2, {
					{ 10, GPUSubresource(seedBuffer) }
				}
			)
		};

		shader = factory.get(
			NAME("Primary shader"),
			Pipeline::Info(
				Pipeline::Flag::NONE,
				"`/shaders/raygen.comp.spv",
				{},
				shaderLayout,
				Vec3u32(THREADS_X, THREADS_Y, 1)
			)
		);
	}

	void PrimaryTask::resize(const Vec2u32 &size) {
		GPUBufferRenderTask::resize(size);
		descriptors->updateDescriptor(9, GPUSubresource(getBuffer()));
		descriptors->flush({ { 9, 1 } });
	}

	void PrimaryTask::switchToScene(SceneGraph *_sceneGraph) { 
		if (sceneGraph != _sceneGraph) {
			markNeedCmdUpdate();
			sceneGraph = _sceneGraph;
		}
	}

	void PrimaryTask::update(f64) {  }

	void PrimaryTask::prepareCommandList(CommandList *cl) {
		cl->add(
			BindDescriptors({ cameraDescriptor, sceneGraph->getDescriptors(), descriptors }),
			BindPipeline(shader),
			Dispatch(size())
		);
	}

}