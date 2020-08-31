#include "rt/task/light_culling_task.hpp"
#include "rt/task/raygen_task.hpp"
#include "rt/enums.hpp"
#include "rt/structs.hpp"
#include "helpers/scene_graph.hpp"
#include "../res/shaders/defines.glsl"

namespace igx::rt {

	LightCullingTask::LightCullingTask(
		RaygenTask *raygen,
		FactoryContainer &factory, 
		const DescriptorsRef &cameraDescriptor
	):
		TextureRenderTask(
			factory.getGraphics(), Texture::Info(
				TextureType::TEXTURE_2D, GPUFormat::outputFormat,
				GPUMemoryUsage::GPU_WRITE_ONLY, 1, 1,1, true
			), NAME("Light culling task")
		),

		raygenTask(raygen),
		factory(factory),
		cameraDescriptor(cameraDescriptor)
	{
		//Setup light culling

		auto raytracingLayout = SceneGraph::getLayout();

		raytracingLayout.push_back(RegisterLayout(
			NAME("Lighting"), 10, TextureType::TEXTURE_2D, 0, 2,
			ShaderAccess::COMPUTE, GPUFormat::rgba16f, true
		));

		raytracingLayout.push_back(RegisterLayout(
			NAME("dirT"), 11, SamplerType::SAMPLER_2D, 1, 2,
			ShaderAccess::COMPUTE
		));

		raytracingLayout.push_back(RegisterLayout(
			NAME("uvObjectNormal"), 12, SamplerType::SAMPLER_2D, 2, 2,
			ShaderAccess::COMPUTE
		));

		layout = factory.get(
			NAME("Light culling shader layout"),
			PipelineLayout::Info(raytracingLayout)
		);

		descriptors = {
			g, NAME("Light culling descriptors"),
			Descriptors::Info(layout, 2, { })
		};

		pipeline = factory.get(
			NAME("Light culling shader"),
			Pipeline::Info(
				Pipeline::Flag::NONE,
				"`/shaders/light_culling.comp.spv",
				{},
				layout,
				Vec3u32(THREADS_XY, THREADS_XY, 1)
			)
		);

		nearestSampler = factory.get(
			NAME("Nearest sampler"),
			Sampler::Info(
				SamplerMin::NEAREST, SamplerMag::NEAREST, SamplerMode::CLAMP_BORDER, 1
			)
		);

	}

	void LightCullingTask::resize(const Vec2u32 &size) {

		Vec2u32 tiles = size.align<THREADS_XY>();

		TextureRenderTask::resize(tiles);

		descriptors->updateDescriptor(10, GPUSubresource(getTexture(), TextureType::TEXTURE_2D));
		descriptors->updateDescriptor(11, GPUSubresource(nearestSampler, raygenTask->getTexture(0), TextureType::TEXTURE_2D));
		descriptors->updateDescriptor(12, GPUSubresource(nearestSampler, raygenTask->getTexture(1), TextureType::TEXTURE_2D));
		descriptors->flush({ { 10, 3 } });
	}

	void LightCullingTask::switchToScene(SceneGraph *_sceneGraph) { 

		if (sceneGraph != _sceneGraph) {
			markNeedCmdUpdate();
			sceneGraph = _sceneGraph;
		}
	}

	void LightCullingTask::update(f64) {  }

	void LightCullingTask::prepareCommandList(CommandList *cl) {
		cl->add(
			BindDescriptors({ cameraDescriptor, sceneGraph->getDescriptors(), descriptors }),
			BindPipeline(pipeline),
			Dispatch(raygenTask->size())
		);
	}

}