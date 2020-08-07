#include "rt/task/main_task.hpp"
#include "rt/task/primary_task.hpp"
#include "rt/enums.hpp"
#include "rt/structs.hpp"
#include "helpers/scene_graph.hpp"
#include "../res/shaders/defines.glsl"

namespace igx::rt {

	CompositeTask::CompositeTask(FactoryContainer &factory, ui::GUI &gui) :

		ParentTextureRenderTask(
			factory.getGraphics(), Texture::Info(
				TextureType::TEXTURE_2D, GPUFormat::outputFormat,
				GPUMemoryUsage::GPU_WRITE_ONLY, 1, 1, 1, true
			), NAME("Main task")
		),

		factory(factory),
		gui(gui)
	{
		seedBuffer = {
			g, NAME("Seed buffer"),
			GPUBuffer::Info(
				sizeof(Seed), GPUBufferType::STORAGE, GPUMemoryUsage::CPU_WRITE
			)
		};

		cameraBuffer = {
			g, NAME("Camera buffer"),
			GPUBuffer::Info(
				sizeof(Camera), GPUBufferType::UNIFORM, GPUMemoryUsage::CPU_WRITE
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

		tasks.add(
			new PrimaryTask(factory, seedBuffer, cameraDescriptor)
		);
	}

	void CompositeTask::resize(const Vec2u32 &size) {

		ParentTextureRenderTask::resize(size);

		camera.width = size.x;
		camera.height = size.y;
		camera.invRes = Vec2f32(1.f / size.x, 1.f / size.y);

	}

	void CompositeTask::switchToScene(SceneGraph *sceneGraph) {
		ParentTextureRenderTask::switchToScene(sceneGraph);
	}

	void CompositeTask::update(f64 dt) {

		ParentTextureRenderTask::update(dt);

		Vec3f32 up = camera.eyeDir.cross(Vec3f32(0, 1, 0)).cross(camera.eyeDir);

		auto v = Mat4x3f32::lookDirection(camera.eye, camera.eyeDir, up);

		const f32 aspect = Vec2f32(f32(camera.width), f32(camera.height)).aspect();
		const f32 nearPlane = f32(std::tan(camera.fov * 0.5_deg));

		camera.p0 = v * Vec4f32(-aspect, 1, -nearPlane, 1);
		camera.p1 = v * Vec4f32(aspect, 1, -nearPlane, 1);
		camera.p2 = v * Vec4f32(-aspect, -1, -nearPlane, 1);

		std::memcpy(cameraBuffer->getBuffer(), &camera, sizeof(Camera));
		cameraBuffer->flush(0, sizeof(Camera));

		seed->cpuOffsetX = r.range(-1000.f, 1000.f);
		seed->cpuOffsetY = r.range(-1000.f, 1000.f);

		seedBuffer->flush(0, sizeof(*seed));
	}

	void CompositeTask::prepareCommandList(CommandList *cl) {

		cl->add(
			FlushBuffer(cameraBuffer, factory.getDefaultUploadBuffer()),
			FlushBuffer(seedBuffer, factory.getDefaultUploadBuffer())
		);

		ParentTextureRenderTask::prepareCommandList(cl);
	}

}