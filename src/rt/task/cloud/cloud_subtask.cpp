#include "rt/task/cloud/cloud_subtask.hpp"
#include "rt/enums.hpp"
#include "rt/structs.hpp"
#include "helpers/scene_graph.hpp"

namespace igx::rt {

	CloudSubtask::CloudSubtask(
		FactoryContainer &factory, const String &name, bool, RaygenTask *primaries, 
		List<RegisterLayout> layouts, const DescriptorsRef &cloudDescriptors,
		const GPUBufferRef &uniformBuffer, const TextureRef &noiseOutputHQ, const TextureRef &noiseOutputLQ,
		const DescriptorsRef &cameraDescriptor
	):
		TextureRenderTask(
			factory.getGraphics(),
			Texture::Info(TextureType::TEXTURE_2D, GPUFormat::outputFormat, GPUMemoryUsage::GPU_WRITE_ONLY), 
			name
		),

		factory(factory), primaries(primaries), cloudDescriptors(cloudDescriptors),
		uniformBuffer(uniformBuffer), noiseOutputHQ(noiseOutputHQ), noiseOutputLQ(noiseOutputLQ),
		cameraDescriptor(cameraDescriptor)
	{
		layouts.push_back(RegisterLayout(
			NAME("Output"), 5, TextureType::TEXTURE_2D, 0, 2, ShaderAccess::COMPUTE, GPUFormat::outputFormat, true
		));

		shaderLayout = factory.get(NAME(name + " layout"), layouts);

		shader = factory.get( 
			NAME(name + " shader"), 
			Pipeline::Info(
				Pipeline::Flag::NONE,
				"`/shaders/clouds.comp.spv",
				{},
				shaderLayout,
				Vec3u32(THREADS_XY, THREADS_XY, 1)
			)
		);
	}

	void CloudSubtask::resize(const Vec2u32 &size) {

		TextureRenderTask::resize(size);

		outputDescriptor.release();
		outputDescriptor = {
			factory.getGraphics(), NAME(getName() + " output desc"),
			Descriptors::Info(
				shaderLayout,
				2,
				{ { 5, GPUSubresource(getTexture(), TextureType::TEXTURE_2D) }}
			)
		};
	}

	void CloudSubtask::prepareCommandList(CommandList *cl) {
		cl->add(
			BindDescriptors({ cameraDescriptor, cloudDescriptors, outputDescriptor }),
			BindPipeline(shader),
			Dispatch(size())
		);
	}
}