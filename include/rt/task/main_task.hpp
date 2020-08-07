#pragma once
#include "utils/random.hpp"
#include "helpers/render_task.hpp"
#include "helpers/factory.hpp"
#include "gui/gui.hpp"
#include "gui/struct_inspector.hpp"
#include "rt/structs.hpp"
#include "../res/shaders/defines.glsl"

namespace igx::rt {

	struct Seed;

	class CompositeTask : public ParentTextureRenderTask {

		static constexpr Vec3u16 noiseResHQ = 128, noiseResLQ = 32;

		ui::GUI &gui;
		FactoryContainer &factory;

		DescriptorsRef descriptors, cameraDescriptor;
		GPUBufferRef seedBuffer, cameraBuffer;

		CPUCamera camera;

		Seed *seed;

		oic::Random r;

	public:

		CompositeTask(FactoryContainer &factory, ui::GUI &gui);

		void prepareCommandList(CommandList *cl) override;

		void update(f64 dt) override;
		void resize(const Vec2u32 &size) override;
		void switchToScene(SceneGraph *sceneGraph) override;

	};

}