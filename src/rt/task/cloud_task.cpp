#include "rt/task/cloud_task.hpp"
#include "rt/enums.hpp"
#include "rt/structs.hpp"
#include "helpers/scene_graph.hpp"

namespace igx::rt {

	//TODO: Fix cloud task (Hit buffer is not appropriate anymore)

	CloudTask::CloudTask(
		GPUBufferRenderTask *primaries,
		FactoryContainer &factory, ui::GUI &gui
	) :
		ParentTextureRenderTask(
			factory.getGraphics(), Texture::Info(
				TextureType::TEXTURE_2D, GPUFormat::outputFormat, GPUMemoryUsage::GPU_WRITE_ONLY, 1, 1, 1, true
			), NAME("Cloud task")
		),

		gui(gui),
		factory(factory),
		primaries(primaries)
	{
		//Setup shader

		shaderLayout = factory.get(
			NAME("Cloud shader layout"),
			PipelineLayout::Info(

				RegisterLayout(
					NAME("Cloud uniform data"), 0,
					GPUBufferType::UNIFORM, 0, 0, ShaderAccess::COMPUTE,
					sizeof(CloudUniformBuffer)
				),

				RegisterLayout(
					NAME("Noise sampler"), 1,
					SamplerType::SAMPLER_3D, 0, 0, ShaderAccess::COMPUTE
				),

				RegisterLayout(
					NAME("Noise sampler HQ"), 2,
					SamplerType::SAMPLER_3D, 0, 0, ShaderAccess::COMPUTE
				),

				RegisterLayout(
					NAME("Hit buffer"), 3,
					GPUBufferType::STRUCTURED, 0, 0, 
					ShaderAccess::COMPUTE, 0, false
				),

				RegisterLayout(
					NAME("Cloutput"), 4,
					TextureType::TEXTURE_2D, 0, 0, 
					ShaderAccess::COMPUTE, GPUFormat::outputFormat, true
				),

				RegisterLayout(
					NAME("Lights"), 5,
					GPUBufferType::STRUCTURED, 1, 0, 
					ShaderAccess::COMPUTE, sizeof(Light), false
				)
			)
		);

		shader = factory.get(
			NAME("Cloud shader"),
			Pipeline::Info(
				Pipeline::Flag::NONE,
				"`/shaders/clouds.comp.spv",
				{},
				shaderLayout,
				Vec3u32(THREADS_XY, THREADS_XY, 1)
			)
		);

		//Set up buffer

		uniformBuffer = {
			factory.getGraphics(), NAME("Cloud uniform buffer"),
			GPUBuffer::Info(
				sizeof(CloudUniformBuffer), GPUBufferType::UNIFORM, GPUMemoryUsage::CPU_WRITE
			)
		};

		cloudData = (CloudUniformBuffer*) uniformBuffer->getBuffer();
		::new(cloudData) CloudUniformBuffer{};

		//Set up noise output
		
		noiseOutputHQ = {
			factory.getGraphics(), NAME("Noise output HQ"),
			Texture::Info(
				noiseResHQ, GPUFormat::rgba8, GPUMemoryUsage::GPU_WRITE_ONLY, 1
			)
		};
		
		noiseOutputLQ = {
			factory.getGraphics(), NAME("Noise output LQ"),
			Texture::Info(
				noiseResLQ, GPUFormat::rgba8, GPUMemoryUsage::GPU_WRITE_ONLY, 1
			)
		};

		linearSampler = factory.get(
			NAME("Linear sampler"),
			Sampler::Info(
				SamplerMin::LINEAR, SamplerMag::LINEAR,
				SamplerMode::REPEAT,
				1.f
			)
		);

		//Set up descriptors

		descriptors = {
			factory.getGraphics(), NAME("Cloud descriptors"),
			Descriptors::Info(
				shaderLayout, 0, {
					{ 0, GPUSubresource(uniformBuffer) },
					{ 1, GPUSubresource(linearSampler, noiseOutputHQ, TextureType::TEXTURE_3D) },
					{ 2, GPUSubresource(linearSampler, noiseOutputLQ, TextureType::TEXTURE_3D) }
				}
			)
		};

		/*
		tasks.add(
			new WorleyTask(factory, NAME("Large scale worley"), noiseOutputHQ, noiseOutputLQ, BlendState::WriteMask::R)
		);
		*/

		/*gui.addWindow(ui::Window(
			"Cloud editor", usz(Editors::CLOUDS), Vec2f32(0, 500), Vec2f32(300, 500),
			&inspector, ui::Window::Flags::DEFAULT_SCROLL_NO_CLOSE
		));*/
	}

	void CloudTask::resize(const Vec2u32 &size) {

		ParentTextureRenderTask::resize(size);

		cloudData->res = size;

		descriptors->updateDescriptor(3, GPUSubresource(primaries->getBuffer()));
		descriptors->updateDescriptor(4, GPUSubresource(getTexture(), TextureType::TEXTURE_3D));
		descriptors->flush({ { 3, 2 } });
	}

	void CloudTask::switchToScene(SceneGraph *sceneGraph) {
		cloudData->directionalLights = sceneGraph->getInfo().directionalLightCount;
		descriptors->updateDescriptor(5, GPUSubresource(sceneGraph->getBuffer<SceneObjectType::LIGHT>()));
		descriptors->flush({ { 5, 1 } });
	}

	void CloudTask::prepareCommandList(CommandList *cl) {

		ParentTextureRenderTask::prepareCommandList(cl);

		cl->add(
			FlushBuffer(uniformBuffer, factory.getDefaultUploadBuffer()),
			BindDescriptors(descriptors),
			BindPipeline(shader),
			Dispatch(size())
		);
	}

	void CloudTask::update(f64 dt) {
		cloudData->Offset_x += f32(dt * windDirectionX * windSpeed);
		cloudData->Offset_z += f32(dt * windDirectionZ * windSpeed);
		uniformBuffer->flush(0, sizeof(*cloudData));
	}

}