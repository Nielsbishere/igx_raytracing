#include "rt/task/composite_task.hpp"
#include "rt/task/raygen_task.hpp"
#include "rt/task/light_culling_task.hpp"
#include "rt/enums.hpp"
#include "rt/structs.hpp"
#include "helpers/scene_graph.hpp"
#include "../res/shaders/defines.glsl"

namespace igx::rt {

	CompositeTask::CompositeTask(const GPUBufferRef &cameraBuffer, FactoryContainer &factory, ui::GUI &gui) :

		ParentTextureRenderTask(
			factory.getGraphics(), 
			{ NAME("Main task"), NAME("Accumulation buffer") },
			Texture::Info(TextureType::TEXTURE_2D, GPUFormat::rgba8, GPUMemoryUsage::GPU_WRITE_ONLY),
			Texture::Info(TextureType::TEXTURE_2D, GPUFormat::rgba32f, GPUMemoryUsage::GPU_WRITE_ONLY)
		),

		cameraRef(cameraBuffer),
		factory(factory),
		gui(gui)
	{
		//Setting up important shared resources

		seedBuffer = {
			g, NAME("Seed buffer"),
			GPUBuffer::Info(
				sizeof(Seed), GPUBufferType::STORAGE, GPUMemoryUsage::CPU_WRITE | GPUMemoryUsage::GPU_WRITE
			)
		};

		PipelineLayout *layout = factory.get(
			NAME("Scene layout"), 
			PipelineLayout::Info(SceneGraph::getLayout())
		);

		cameraDescriptor = {
			g, NAME("Camera descriptor"),
			Descriptors::Info(
				layout, 0,
				{ { 0, GPUSubresource(cameraBuffer) } }
			)
		};

		seed = (Seed*) seedBuffer->getBuffer();

		//Create descriptors and post processing shader

		auto raytracingLayout = SceneGraph::getLayout();

		raytracingLayout.push_back(RegisterLayout(
			NAME("UI"), 10, SamplerType::SAMPLER_MS, 1, 2,
			ShaderAccess::COMPUTE
		));

		raytracingLayout.push_back(RegisterLayout(
			NAME("rayOutput"), 11, TextureType::TEXTURE_2D, 0, 2,
			ShaderAccess::COMPUTE, GPUFormat::rgba8, true
		));

		raytracingLayout.push_back(RegisterLayout(
			NAME("dirT"), 12, SamplerType::SAMPLER_2D, 2, 2,
			ShaderAccess::COMPUTE
		));

		raytracingLayout.push_back(RegisterLayout(
			NAME("uvObjectNormal"), 13, SamplerType::SAMPLER_2D, 3, 2,
			ShaderAccess::COMPUTE
		));

		raytracingLayout.push_back(RegisterLayout(
			NAME("accumulation"), 14, TextureType::TEXTURE_2D, 1, 2,
			ShaderAccess::COMPUTE, GPUFormat::rgba32f, true
		));

		raytracingLayout.push_back(RegisterLayout(
			NAME("seed"), 15, GPUBufferType::STORAGE, 8, 2,
			ShaderAccess::COMPUTE, sizeof(Seed)
		));

		#ifdef GRAPHICS_DEBUG

			debugBuffer = {
				g, NAME("Debug buffer"),
				GPUBuffer::Info(
					sizeof(DebugData), GPUBufferType::UNIFORM,
					GPUMemoryUsage::CPU_WRITE
				)
			};

			debData = (DebugData*) debugBuffer->getBuffer();
			::new(debData) DebugData();

			raytracingLayout.push_back(RegisterLayout(
				NAME("DebugInfo"), 16, GPUBufferType::UNIFORM, 2, 2,
				ShaderAccess::COMPUTE, sizeof(DebugData)
			));

			gui.addWindow(
				ui::Window(
					"Debug window", EDITOR_DEBUG,
					Vec2f32(0, 500), Vec2f32(360, 100),
					&debData,
					ui::Window::DEFAULT_SCROLL_NO_CLOSE
				)
			);

		#endif

		nearestSampler = factory.get(
			NAME("Nearest sampler"),
			Sampler::Info(
				SamplerMin::NEAREST, SamplerMag::NEAREST, SamplerMode::CLAMP_BORDER, 1
			)
		);

		//Set up composite shader

		shaderLayout = factory.get(
			NAME("Composite shader layout"),
			PipelineLayout::Info(raytracingLayout)
		);

		descriptors = {
			g, NAME("Composite task descriptors"),
			Descriptors::Info(shaderLayout, 2, {

				{ 10, GPUSubresource(nearestSampler, gui.getFramebuffer()->getTarget(0), TextureType::TEXTURE_MS) },
				{ 15, GPUSubresource(seedBuffer) }

				#ifdef GRAPHICS_DEBUG
				, { 16, GPUSubresource(debugBuffer) }
				#endif

			})
		};

		shader = factory.get(
			NAME("Composite shader"),
			Pipeline::Info(
				Pipeline::Flag::NONE,
				"`/shaders/composite.comp.spv",
				{},
				shaderLayout,
				Vec3u32(THREADS_XY, THREADS_XY, 1)
			)
		);

		List<RegisterLayout> initLayout = { RegisterLayout(
			NAME("Seed buffer"), 0, GPUBufferType::STORAGE, 0, 0,
			ShaderAccess::COMPUTE, sizeof(Seed), true
		) };

		//Set up init shader for things like the seed

		initShaderLayout = factory.get(
			NAME("Init shader layout"),
			PipelineLayout::Info(initLayout)
		);

		initDescriptors = {
			g, NAME("Init task descriptors"),
			Descriptors::Info(initShaderLayout, 0, { { 0, GPUSubresource(seedBuffer) } })
		};

		initShader = factory.get(
			NAME("Init shader"),
			Pipeline::Info(
				Pipeline::Flag::NONE,
				"`/shaders/init.comp.spv",
				{},
				initShaderLayout,
				Vec3u32(1, 1, 1)
			)
		);

		//Subtasks

		auto raygen = new RaygenTask(factory, seedBuffer, cameraDescriptor);

		tasks.add(
			raygen /*,
			new LightCullingTask(raygen, factory, cameraDescriptor)*/
		);
	}

	CompositeTask::~CompositeTask() {
		gui.removeWindow(EDITOR_CAMERA);
	}

	void CompositeTask::resize(const Vec2u32 &size) {

		ParentTextureRenderTask::resize(size);

		auto raygen = tasks.get<RaygenTask>(0);

		descriptors->updateDescriptor(11, GPUSubresource(getTexture(0), TextureType::TEXTURE_2D));
		descriptors->updateDescriptor(12, GPUSubresource(nearestSampler, raygen->getTexture(0), TextureType::TEXTURE_2D));
		descriptors->updateDescriptor(13, GPUSubresource(nearestSampler, raygen->getTexture(1), TextureType::TEXTURE_2D));
		descriptors->updateDescriptor(14, GPUSubresource(getTexture(1), TextureType::TEXTURE_2D));
		descriptors->flush({ { 11, 4 } });
	}

	void CompositeTask::switchToScene(SceneGraph *_sceneGraph) {

		ParentTextureRenderTask::switchToScene(_sceneGraph);

		if (sceneGraph != _sceneGraph) {
			markNeedCmdUpdate();
			sceneGraph = _sceneGraph;
		}
	}

	void CompositeTask::update(f64 dt) {

		ParentTextureRenderTask::update(dt);

		seed->sampleCount = 0;
		seed->cpuOffsetX = r.range(-1000.f, 1000.f);
		seed->cpuOffsetY = r.range(-1000.f, 1000.f);

		seedBuffer->flush(0, sizeof(*seed));

		if(debugBuffer)
			debugBuffer->flush(0, sizeof(DebugData));
	}

	void CompositeTask::prepareCommandList(CommandList *cl) {

		//Setup all GPU data

		cl->add(FlushBuffer(seedBuffer, factory.getDefaultUploadBuffer()));

		if(debugBuffer)
			cl->add(FlushBuffer(debugBuffer, factory.getDefaultUploadBuffer()));
		
		cl->add(
			BindDescriptors(initDescriptors),
			BindPipeline(initShader),
			Dispatch(1)
		);

		//All sub tasks

		ParentTextureRenderTask::prepareCommandList(cl);

		//Composite render task

		cl->add(
			BindDescriptors({ cameraDescriptor, sceneGraph->getDescriptors(), descriptors }),
			BindPipeline(shader),
			Dispatch(size())
		);
	}

}