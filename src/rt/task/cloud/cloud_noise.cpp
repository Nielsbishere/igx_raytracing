#include "rt/task/cloud/cloud_noise.hpp"
#include "rt/enums.hpp"
#include "rt/structs.hpp"
#include "helpers/scene_graph.hpp"

namespace igx::rt {

	CloudNoiseTask::CloudNoiseTask(FactoryContainer &factory, const String &name, const GPUBufferRef &data, const TextureRef &output) :
		RenderTask(factory.getGraphics()), factory(factory), output(output), data(data)
	{
		Vec3u32 inf = output->getDimensions().cast<Vec3u32>();

		targetRes = {
			factory.getGraphics(), NAME(name + " target res"),
			GPUBuffer::Info(Buffer((u8*)&inf, (u8*)(&inf + 1)), GPUBufferType::UNIFORM, GPUMemoryUsage::LOCAL)
		};

		layout = factory.get(
			NAME(name + " layout"), {
				RegisterLayout(NAME("NoiseData"), 0, GPUBufferType::UNIFORM, 0, 0, ShaderAccess::COMPUTE, data->size()),
				RegisterLayout(NAME("Target"), 1, GPUBufferType::UNIFORM, 1, 0, ShaderAccess::COMPUTE, sizeof(inf)),
				RegisterLayout(NAME("worleyOutput"), 2, TextureType::TEXTURE_3D, 0, 0, ShaderAccess::COMPUTE, GPUFormat::r8, true)
			}
		);

		descriptors = {
			factory.getGraphics(), NAME(name + " descriptors"),
			Descriptors::Info(
				layout, 0, {
					{ 0, GPUSubresource(data) },
					{ 1, GPUSubresource(targetRes) },
					{ 2, GPUSubresource(output, TextureType::TEXTURE_3D) }
				}
			)
		};

		shader = {
			factory.getGraphics(), NAME(name + " shader"),
			Pipeline::Info(
				Pipeline::Flag::NONE,
				"`/shaders/cloud_noise.comp.spv",
				{},
				layout,
				Vec3u32(THREADS_XY, THREADS_XY, 1)
			)
		};
	}

	void CloudNoiseTask::prepareCommandList(CommandList *cl) {
		cl->add(
			FlushBuffer(targetRes, factory.getDefaultUploadBuffer()),
			BindDescriptors(descriptors),
			BindPipeline(shader),
			Dispatch(output->getDimensions().cast<Vec3u32>())
		);
	}
}