#include "rt/task/raygen_task.hpp"
#include "rt/enums.hpp"
#include "rt/structs.hpp"
#include "helpers/scene_graph.hpp"
#include "../res/shaders/defines.glsl"

namespace igx::rt {

	RaygenTask::RaygenTask(
		FactoryContainer &factory, 
		const GPUBufferRef &seedBuffer,
		const DescriptorsRef &cameraDescriptor
	) :
		GPUBufferRenderTask(
			factory.getGraphics(), GPUBuffer::Info(
				sizeof(Hit), GPUBufferType::STRUCTURED, GPUMemoryUsage::GPU_WRITE_ONLY
			), NAME("Raygen task")
		),

		factory(factory),
		seedBuffer(seedBuffer),
		cameraDescriptor(cameraDescriptor)
	{
		//Setup shader

		auto raytracingLayout = SceneGraph::getLayout();

		raytracingLayout.push_back(RegisterLayout(
			NAME("HitBuffer"), 10, GPUBufferType::STRUCTURED, 7, 2,
			ShaderAccess::COMPUTE, sizeof(Hit), true
		));

		raytracingLayout.push_back(RegisterLayout(
			NAME("SeedBuffer"), 11, GPUBufferType::STORAGE, 8, 2,
			ShaderAccess::COMPUTE, sizeof(Seed)
		));

		shaderLayout = factory.get(
			NAME("Raygen shader layout"),
			PipelineLayout::Info(raytracingLayout)
		);

		descriptors = {
			g, NAME("Raygen task descriptors"),
			Descriptors::Info(
				shaderLayout, 2, {
					{ 11, GPUSubresource(seedBuffer) }
				}
			)
		};

		shader = factory.get(
			NAME("Raygen shader"),
			Pipeline::Info(
				Pipeline::Flag::NONE,
				"`/shaders/raygen.comp.spv",
				{},
				shaderLayout,
				Vec3u32(THREADS_XY, THREADS_XY, 1)
			)
		);
	}

	void RaygenTask::resize(const Vec2u32 &size) {
		GPUBufferRenderTask::resize(size.align<THREADS_XY>());
		descriptors->updateDescriptor(10, GPUSubresource(getBuffer()));
		descriptors->flush({ { 10, 1 } });
	}

	void RaygenTask::switchToScene(SceneGraph *_sceneGraph) { 
		if (sceneGraph != _sceneGraph) {
			markNeedCmdUpdate();
			sceneGraph = _sceneGraph;
		}
	}

	void RaygenTask::update(f64) {  }

	void RaygenTask::prepareCommandList(CommandList *cl) {
		cl->add(
			BindDescriptors({ cameraDescriptor, sceneGraph->getDescriptors(), descriptors }),
			BindPipeline(shader),
			Dispatch(size())
		);
	}

}