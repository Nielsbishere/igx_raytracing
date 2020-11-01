#include "rt/task/cloud/cloud_task.hpp"
#include "rt/task/cloud/cloud_subtask.hpp"
#include "rt/task/cloud/cloud_noise.hpp"
#include "rt/enums.hpp"
#include "rt/structs.hpp"
#include "helpers/scene_graph.hpp"

namespace igx::rt {

	//TODO: Fix cloud task (Hit buffer is not appropriate anymore)

	CloudTask::CloudTask(
		RaygenTask *primaries,
		FactoryContainer &factory, ui::GUI &gui,
		const DescriptorsRef &camera
	) :
		RenderTask(factory.getGraphics(), NAME("Cloud task"), Vec4f32(1, 1, 1, 1)),
		gui(gui),
		factory(factory),
		primaries(primaries)
	{

		//Set up buffers and samplers

		uniformBuffer = {
			factory.getGraphics(), NAME("Cloud uniform buffer"),
			GPUBuffer::Info(
				sizeof(CloudBuffer), GPUBufferUsage::UNIFORM, GPUMemoryUsage::CPU_WRITE
			)
		};

		noiseUniformData = {
			factory.getGraphics(), NAME("Noise uniform buffer"),
			GPUBuffer::Info(
				sizeof(NoiseUniformData), GPUBufferUsage::UNIFORM, GPUMemoryUsage::CPU_WRITE
			)
		};

		linearSampler = factory.get(
			NAME("Linear repeat sampler"), Sampler::Info(SamplerMin::LINEAR, SamplerMag::LINEAR, SamplerMode::REPEAT, 1.f)
		);

		//Set up noise output
		
		noiseOutputHQ = {
			factory.getGraphics(), NAME("Noise output HQ"),
			Texture::Info(
				noiseResHQ, GPUFormat::r8, GPUMemoryUsage::GPU_WRITE_ONLY, 1
			)
		};
		
		noiseOutputLQ = {
			factory.getGraphics(), NAME("Noise output LQ"),
			Texture::Info(
				noiseResLQ, GPUFormat::r8, GPUMemoryUsage::GPU_WRITE_ONLY, 1
			)
		};

		List<RegisterLayout> cloudLayouts = {
			RegisterLayout(NAME("worleySampler"),	1, SamplerType::SAMPLER_3D,		0, 1, ShaderAccess::COMPUTE),
			RegisterLayout(NAME("dirT"),			2, SamplerType::SAMPLER_2D,		1, 1, ShaderAccess::COMPUTE),
			RegisterLayout(NAME("CloudBuffer"),		3, GPUBufferType::UNIFORM,		1, 1, ShaderAccess::COMPUTE, sizeof(CloudBuffer)),
			RegisterLayout(NAME("Lights"),			4, GPUBufferType::STRUCTURED,	0, 1, ShaderAccess::COMPUTE, sizeof(Light))
		};

		cloudLayout = factory.get(NAME("Cloud layout"), cloudLayouts);

		cloudDescriptors = {
			factory.getGraphics(), NAME("Cloud descriptors"),
			Descriptors::Info(
				cloudLayout, 1,
				{
					{ 1, GPUSubresource(linearSampler, noiseOutputHQ, TextureType::TEXTURE_3D) },
					{ 3, GPUSubresource(uniformBuffer, GPUBufferType::UNIFORM) }
				}
			)
		};

		tasks.add(

			new CloudNoiseTask(factory, NAME("Cloud noise HQ"), noiseUniformData, noiseOutputHQ),
			new CloudNoiseTask(factory, NAME("Cloud noise LQ"), noiseUniformData, noiseOutputLQ),

			subtasks[0] = new CloudSubtask(
				factory, NAME("Cloud subtask"), false, primaries,
				cloudLayouts, cloudDescriptors, uniformBuffer, noiseOutputHQ, noiseOutputLQ, camera
			)
		);

		gui.addWindow(ui::Window(
			"Cloud editor", EDITOR_CLOUDS, Vec2f32(0, 600), Vec2f32(300, 300),
			&cloudBuffer, ui::Window::Flags::DEFAULT_SCROLL_NO_CLOSE
		));

		gui.addWindow(ui::Window(
			"Cloud noise editor", EDITOR_CLOUD_NOISE, Vec2f32(0, 900), Vec2f32(300, 180),
			&noiseUniforms, ui::Window::Flags::DEFAULT_SCROLL_NO_CLOSE
		));
	}

	CloudTask::~CloudTask() {
		gui.removeWindow(EDITOR_CLOUDS);
	}

	bool CloudTask::needsCommandUpdate() const {

		if (RenderTask::needsCommandUpdate())
			return true;

		for (RenderTask *task : tasks)
			if (task->needsCommandUpdate())
				return true;

		return false;
	}

	void CloudTask::resize(const Vec2u32 &size) {

		RenderTask::resize(size);
		tasks.resize(size);

		cloudDescriptors->updateDescriptor(2, GPUSubresource(linearSampler, primaries->getTexture(), TextureType::TEXTURE_2D));
		cloudDescriptors->flush({ { 2, 1 } });
	}

	void CloudTask::switchToScene(SceneGraph *sceneGraph) {

		tasks.switchToScene(sceneGraph);

		cloudBuffer->directionalLights = sceneGraph->getInfo().directionalLightCount;

		cloudDescriptors->updateDescriptor(4, GPUSubresource(
			sceneGraph->getBuffer<SceneObjectType::LIGHT>(), GPUBufferType::STRUCTURED, 0, cloudBuffer->directionalLights * sizeof(Light)
		));
		cloudDescriptors->flush({ { 4, 1 } });
	}

	void CloudTask::prepareCommandList(CommandList * /*cl*/) {

		/* TODO: Make Clouds more efficient

		cl->add(
			FlushBuffer(uniformBuffer, factory.getDefaultUploadBuffer()),
			FlushBuffer(noiseUniformData, factory.getDefaultUploadBuffer())
		);

		tasks.prepareCommandList(cl); */
	}

	void CloudTask::update(f64 dt) {

		tasks.update(dt);

		cloudBuffer->Offset_x += f32(dt * cloudBuffer->Wind_direction_x * cloudBuffer->Wind_speed);
		cloudBuffer->Offset_z += f32(dt * cloudBuffer->Wind_direction_z * cloudBuffer->Wind_speed);

		std::memcpy(uniformBuffer->getBuffer(), &cloudBuffer.value, sizeof(CloudBuffer));
		uniformBuffer->flush(0, sizeof(CloudBuffer));

		std::memcpy(noiseUniformData->getBuffer(), &noiseUniforms.value, sizeof(NoiseUniformData));
		noiseUniformData->flush(0, sizeof(NoiseUniformData));
	}

	Texture *CloudTask::getOutput(bool) const {
		return subtasks[0]->getTexture();
	}

}