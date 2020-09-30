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
		const GPUBufferRef &seed,
		const DescriptorsRef &cameraDescriptor
	) :
		TextureRenderTask(
			factory.getGraphics(),
			NAME("Shadow task"),
			Vec4f32(0, 0, 0, 1),
			{ NAME("Lighting"), NAME("History") },
			Texture::Info(TextureType::TEXTURE_2D, GPUFormat::outputFormat, GPUMemoryUsage::GPU_WRITE_ONLY), 
			Texture::Info(TextureType::TEXTURE_2D, GPUFormat::outputFormat, GPUMemoryUsage::GPU_WRITE_ONLY)
		),

		factory(factory),
		raygen(raygen),
		cameraDescriptor(cameraDescriptor),
		seed(seed)
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

		linearSampler = factory.get(
			NAME("Linear wrap sampler"),
			Sampler::Info(
				SamplerMin::LINEAR, SamplerMag::LINEAR, SamplerMode::REPEAT, 1.f
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

		usz stride = g.getVendor() == Vendor::NVIDIA ? sizeof(u32) : sizeof(u64);

		raytracingLayout.push_back(RegisterLayout(
			NAME("ShadowOutput"), 13, GPUBufferType::STRUCTURED, 7, 2, ShaderAccess::COMPUTE, stride, true
		));

		raytracingLayout.push_back(RegisterLayout(
			NAME("dirObject"), 14, SamplerType::SAMPLER_2D, 1, 2, ShaderAccess::COMPUTE
		));

		raytracingLayout.push_back(RegisterLayout(
			NAME("uvNormal"), 15, SamplerType::SAMPLER_2D, 2, 2, ShaderAccess::COMPUTE
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
					{ 11, GPUSubresource(seed) }
				}
			)
		};

		String ext = ".spv";

		if (g.getVendor() == Vendor::NVIDIA)
			ext = ".nv.spv";

		shadowShader = factory.get(
			NAME("Shadow shader"),
			Pipeline::Info(
				Pipeline::Flag::NONE,
				"`/shaders/shadow.comp" + ext,
				{},
				shadowLayout,
				Vec3u32(THREADS_XY, THREADS_XY, 2)
			)
		);

		//Lighting shader

		raytracingLayout[13].isWritable = false;

		raytracingLayout.push_back(RegisterLayout(
			NAME("lighting"), 16, TextureType::TEXTURE_2D, 0, 2, 
			ShaderAccess::COMPUTE, GPUFormat::outputFormat, true
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
					{ 11, GPUSubresource(seed) }
				}
			)
		};

		lightingShader = factory.get(
			NAME("Lighting shader"),
			Pipeline::Info(
				Pipeline::Flag::NONE,
				"`/shaders/lighting.comp" + ext,
				{},
				lightingLayout,
				Vec3u32(THREADS_XY, THREADS_XY, 1)
			)
		);
	}

	void ShadowTask::resize(const Vec2u32 &size) {

		TextureRenderTask::resize(size);

		u32 threadsY = THREADS_64_OVER_XY;
		usz stride = sizeof(u64);

		if (g.getVendor() == Vendor::NVIDIA) {
			threadsY = 32 / THREADS_XY;
			stride = sizeof(u32);
		}

		auto warps = (size.cast<Vec2f32>() / Vec2f32(THREADS_XY, threadsY)).ceil().cast<Vec2u16>();
		usz warps1D = warps.prod<usz>() * properties->Shadow_samples;

		shadowOutput.release();
		shadowOutput = {
			factory.getGraphics(), NAME("Shadow output"),
			GPUBuffer::Info(stride * warps1D, GPUBufferType::STRUCTURED, GPUMemoryUsage::GPU_WRITE_ONLY)
		};

		shadowDescriptors->updateDescriptor(13, GPUSubresource(shadowOutput));
		shadowDescriptors->updateDescriptor(14, GPUSubresource(nearestSampler, raygen->getTexture(0), TextureType::TEXTURE_2D));
		shadowDescriptors->flush({ { 13, 2 } });

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