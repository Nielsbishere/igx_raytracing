#include "rt/task/raygen_task.hpp"
#include "rt/task/shadow_task.hpp"
#include "rt/enums.hpp"
#include "rt/structs.hpp"
#include "helpers/scene_graph.hpp"
#include "../res/shaders/defines.glsl"

namespace igx::rt {

	ShadowTask::ShadowTask(
		FactoryContainer &factory,
		RaygenTask *raygen,
		const TextureRef &blueNoise,
		const GPUBufferRef &seed,
		const DescriptorsRef &cameraDescriptor
	) :
		TextureRenderTask(
			factory.getGraphics(), 
			{ NAME("Lighting"), NAME("History") },
			Texture::Info(TextureType::TEXTURE_2D, GPUFormat::outputFormat, GPUMemoryUsage::GPU_WRITE_ONLY), 
			Texture::Info(TextureType::TEXTURE_2D, GPUFormat::outputFormat, GPUMemoryUsage::GPU_WRITE_ONLY)
		),

		factory(factory),
		raygen(raygen),
		cameraDescriptor(cameraDescriptor),
		seed(seed),
		blueNoise(blueNoise)
	{
		//Setup uniforms and samplers

		shadowProperties = {
			factory.getGraphics(), NAME("Shadow properties"),
			GPUBuffer::Info(sizeof(ShadowProperties), GPUBufferType::UNIFORM, GPUMemoryUsage::CPU_WRITE)
		};

		nearestSampler = factory.get(
			NAME("Nearest clamp sampler"),
			Sampler::Info(
				SamplerMin::NEAREST, SamplerMag::NEAREST, SamplerMode::CLAMP_BORDER, 1.f
			)
		);

		nearestWrapSampler = factory.get(
			NAME("Nearest wrap sampler"),
			Sampler::Info(
				SamplerMin::NEAREST, SamplerMag::NEAREST, SamplerMode::REPEAT, 1.f
			)
		);

		//Setup shared registers

		auto raytracingLayout = SceneGraph::getLayout();

		raytracingLayout.push_back(RegisterLayout(
			NAME("ShadowProperties"), 10, GPUBufferType::UNIFORM, 2, 2, ShaderAccess::COMPUTE, sizeof(ShadowProperties)
		));

		raytracingLayout.push_back(RegisterLayout(
			NAME("Seed"), 11, GPUBufferType::STORAGE, 8, 2, ShaderAccess::COMPUTE, sizeof(Seed)
		));

		raytracingLayout.push_back(RegisterLayout(
			NAME("blueNoise"), 12, SamplerType::SAMPLER_2D, 3, 2, ShaderAccess::COMPUTE
		));

		raytracingLayout.push_back(RegisterLayout(
			NAME("ShadowOutput"), 13, GPUBufferType::STRUCTURED, 7, 2, ShaderAccess::COMPUTE, sizeof(u64), true
		));

		raytracingLayout.push_back(RegisterLayout(
			NAME("dirT"), 14, SamplerType::SAMPLER_2D, 1, 2, ShaderAccess::COMPUTE
		));

		raytracingLayout.push_back(RegisterLayout(
			NAME("uvObjectNormal"), 15, SamplerType::SAMPLER_2D, 2, 2, ShaderAccess::COMPUTE
		));

		//Setup shadow

		shadowLayout = factory.get(
			NAME("Shadow layout"),
			PipelineLayout::Info(raytracingLayout)
		);

		shadowDescriptors = {
			g, NAME("Shadow descriptors"),
			Descriptors::Info(
				shadowLayout, 2, {
					{ 10, GPUSubresource(shadowProperties) },
					{ 11, GPUSubresource(seed) },
					{ 12, GPUSubresource(nearestWrapSampler, blueNoise, TextureType::TEXTURE_2D) }
				}
			)
		};

		shadowShader = factory.get(
			NAME("Shadow shader"),
			Pipeline::Info(
				Pipeline::Flag::NONE,
				"`/shaders/shadow.comp.spv",
				{},
				shadowLayout,
				Vec3u32(THREADS_XY, THREADS_XY, 1)
			)
		);

		//Lighting shader

		raytracingLayout[13].isWritable = false;

		raytracingLayout.push_back(RegisterLayout(
			NAME("lighting"), 16, TextureType::TEXTURE_2D, 0, 2, ShaderAccess::COMPUTE, GPUFormat::outputFormat, true
		));

		lightingLayout = factory.get(
			NAME("Lighting layout"),
			PipelineLayout::Info(raytracingLayout)
		);

		lightingDescriptors = {
			g, NAME("Lighting descriptors"),
			Descriptors::Info(
				lightingLayout, 2, {
					{ 10, GPUSubresource(shadowProperties) },
					{ 11, GPUSubresource(seed) },
					{ 12, GPUSubresource(nearestWrapSampler, blueNoise, TextureType::TEXTURE_2D) }
				}
			)
		};

		lightingShader = factory.get(
			NAME("Lighting shader"),
			Pipeline::Info(
				Pipeline::Flag::NONE,
				"`/shaders/lighting.comp.spv",
				{},
				lightingLayout,
				Vec3u32(THREADS_XY, THREADS_XY, 1)
			)
		);
	}

	void ShadowTask::resize(const Vec2u32 &size) {

		TextureRenderTask::resize(size);

		auto warps = (size.cast<Vec2f32>() / Vec2f32(THREADS_XY, THREADS_64_OVER_XY)).ceil().cast<Vec2u16>();
		usz warps1D = warps.prod<usz>();

		shadowOutput.release();
		shadowOutput = {
			factory.getGraphics(), NAME("Shadow output"),
			GPUBuffer::Info(sizeof(u64) * warps1D, GPUBufferType::STRUCTURED, GPUMemoryUsage::GPU_WRITE_ONLY)
		};

		shadowDescriptors->updateDescriptor(13, GPUSubresource(shadowOutput));
		shadowDescriptors->updateDescriptor(14, GPUSubresource(nearestSampler, raygen->getTexture(0), TextureType::TEXTURE_2D));
		shadowDescriptors->updateDescriptor(15, GPUSubresource(nearestSampler, raygen->getTexture(1), TextureType::TEXTURE_2D));
		shadowDescriptors->flush({ { 13, 3 } });

		lightingDescriptors->updateDescriptor(13, GPUSubresource(shadowOutput));
		lightingDescriptors->updateDescriptor(14, GPUSubresource(nearestSampler, raygen->getTexture(0), TextureType::TEXTURE_2D));
		lightingDescriptors->updateDescriptor(15, GPUSubresource(nearestSampler, raygen->getTexture(1), TextureType::TEXTURE_2D));
		lightingDescriptors->updateDescriptor(16, GPUSubresource(getTexture(0), TextureType::TEXTURE_2D));
		lightingDescriptors->flush({ { 13, 4 } });
	}

	void ShadowTask::switchToScene(SceneGraph *_sceneGraph) { 
		if (sceneGraph != _sceneGraph) {
			markNeedCmdUpdate();
			sceneGraph = _sceneGraph;
		}
	}

	bool ShadowTask::needsCommandUpdate() const {
		return TextureRenderTask::needsCommandUpdate() || cachedSamples != properties->Shadow_samples;
	}

	void ShadowTask::update(f64) {
		std::memcpy(shadowProperties->getBuffer(), &properties.value, sizeof(ShadowProperties));
		shadowProperties->flush(0, sizeof(ShadowProperties));
	}

	void ShadowTask::prepareCommandList(CommandList *cl) {

		cl->add(

			FlushBuffer(shadowProperties, factory.getDefaultUploadBuffer()),

			//Trace shadow rays

			BindDescriptors({ cameraDescriptor, sceneGraph->getDescriptors(), shadowDescriptors }),
			BindPipeline(shadowShader),
			Dispatch(Vec3u32(size().x, size().y, cachedSamples = properties->Shadow_samples)),

			//Do lighting 

			BindDescriptors({ cameraDescriptor, sceneGraph->getDescriptors(), lightingDescriptors }),
			BindPipeline(lightingShader),
			Dispatch(Vec2u32(size().x, size().y))

			//Do denoising

		);
	}

}