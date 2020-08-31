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
			factory.getGraphics(), Texture::Info(
				TextureType::TEXTURE_2D, GPUFormat::rgba8,
				GPUMemoryUsage::GPU_WRITE_ONLY, 1, 1, 1, true
			), NAME("Main task")
		),

		cameraRef(cameraBuffer),
		factory(factory),
		gui(gui)
	{
		//Setting up important shared resources

		seedBuffer = {
			g, NAME("Seed buffer"),
			GPUBuffer::Info(
				sizeof(Seed), GPUBufferType::STORAGE, GPUMemoryUsage::CPU_WRITE
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
			NAME("rayOutput"), 10, TextureType::TEXTURE_2D, 0, 2,
			ShaderAccess::COMPUTE, GPUFormat::rgba8, true
		));

		raytracingLayout.push_back(RegisterLayout(
			NAME("UI"), 11, SamplerType::SAMPLER_MS, 1, 2,
			ShaderAccess::COMPUTE
		));

		raytracingLayout.push_back(RegisterLayout(
			NAME("dirT"), 12, SamplerType::SAMPLER_2D, 2, 2,
			ShaderAccess::COMPUTE
		));

		raytracingLayout.push_back(RegisterLayout(
			NAME("uvObjectNormal"), 13, SamplerType::SAMPLER_2D, 3, 2,
			ShaderAccess::COMPUTE
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
				NAME("DebugInfo"), 14, GPUBufferType::UNIFORM, 2, 2,
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

		shaderLayout = factory.get(
			NAME("Composite shader layout"),
			PipelineLayout::Info(raytracingLayout)
		);

		descriptors = {
			g, NAME("Composite task descriptors"),
			Descriptors::Info(shaderLayout, 2, {

				#ifdef GRAPHICS_DEBUG
					{ 14, GPUSubresource(debugBuffer) }
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

		nearestSampler = factory.get(
			NAME("Nearest sampler"),
			Sampler::Info(
				SamplerMin::NEAREST, SamplerMag::NEAREST, SamplerMode::CLAMP_BORDER, 1
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

		descriptors->updateDescriptor(10, GPUSubresource(getTexture(), TextureType::TEXTURE_2D));
		descriptors->updateDescriptor(11, GPUSubresource(nearestSampler, gui.getFramebuffer()->getTarget(0), TextureType::TEXTURE_MS));
		descriptors->updateDescriptor(12, GPUSubresource(nearestSampler, raygen->getTexture(0), TextureType::TEXTURE_2D));
		descriptors->updateDescriptor(13, GPUSubresource(nearestSampler, raygen->getTexture(1), TextureType::TEXTURE_2D));
		descriptors->flush({ { 10, 4 } });
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

		seed->cpuOffsetX = r.range(-1000.f, 1000.f);
		seed->cpuOffsetY = r.range(-1000.f, 1000.f);

		seedBuffer->flush(0, sizeof(*seed));

		if(debugBuffer)
			debugBuffer->flush(0, sizeof(DebugData));
	}

	void CompositeTask::prepareCommandList(CommandList *cl) {

		//Setup all buffer CPU data

		cl->add(FlushBuffer(seedBuffer, factory.getDefaultUploadBuffer()));

		if(debugBuffer)
			cl->add(FlushBuffer(debugBuffer, factory.getDefaultUploadBuffer()));
		
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