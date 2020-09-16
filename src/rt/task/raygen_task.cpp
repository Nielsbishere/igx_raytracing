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
		TextureRenderTask(
			factory.getGraphics(), 
			{ NAME("dirT"), NAME("uvObjectNormal") },
			Texture::Info(TextureType::TEXTURE_2D, GPUFormat::rgba32f, GPUMemoryUsage::GPU_WRITE_ONLY), 
			Texture::Info(TextureType::TEXTURE_2D, GPUFormat::rgba32f, GPUMemoryUsage::GPU_WRITE_ONLY)
		),

		factory(factory),
		seedBuffer(seedBuffer),
		cameraDescriptor(cameraDescriptor)
	{
		//Setup shader

		auto raytracingLayout = SceneGraph::getLayout();

		raytracingLayout.push_back(RegisterLayout(
			NAME("DirT buffer"), 10, TextureType::TEXTURE_2D, 0, 2,
			ShaderAccess::COMPUTE, GPUFormat::rgba32f, true
		));

		raytracingLayout.push_back(RegisterLayout(
			NAME("UvObjectNormal buffer"), 11, TextureType::TEXTURE_2D, 1, 2,
			ShaderAccess::COMPUTE, GPUFormat::rgba32f, true
		));

		raytracingLayout.push_back(RegisterLayout(
			NAME("SeedBuffer"), 12, GPUBufferType::STORAGE, 8, 2,
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
					{ 12, GPUSubresource(seedBuffer) }
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

		TextureRenderTask::resize(size);

		descriptors->updateDescriptor(10, GPUSubresource(getTexture(0), TextureType::TEXTURE_2D));
		descriptors->updateDescriptor(11, GPUSubresource(getTexture(1), TextureType::TEXTURE_2D));
		descriptors->flush({ { 10, 2 } });
	}

	void RaygenTask::switchToScene(SceneGraph *_sceneGraph) { 
		if (sceneGraph != _sceneGraph) {
			markNeedCmdUpdate();
			sceneGraph = _sceneGraph;
		}
	}

	void RaygenTask::prepareCommandList(CommandList *cl) {
		cl->add(
			BindDescriptors({ cameraDescriptor, sceneGraph->getDescriptors(), descriptors }),
			BindPipeline(shader),
			Dispatch(size())
		);
	}

}