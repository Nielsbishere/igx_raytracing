#pragma once
#include "utils/random.hpp"
#include "helpers/render_task.hpp"
#include "helpers/factory.hpp"
#include "gui/struct_inspector.hpp"
#include "rt/structs.hpp"
#include "../res/shaders/defines.glsl"

namespace igx::rt {

	struct Seed;

	oicExposedEnum(
		DebugType,
		u32,
		Default,
		Ray_direction,
		Normal,
		Uv,
		Object,
		Primitive,
		Material,
		Albedo,
		Metallic,
		Ambient,
		Roughness,
		Emissive,
		Transparency,
		Sky
	);

	struct DebugData {

		DebugType Display_type = DebugType::Normal;
		bool Display_NaN_only{}; u8 pad0[3]{};

		Inflect(Display_type, Display_NaN_only);

	};

	class CompositeTask : public ParentTextureRenderTask {

		static constexpr Vec3u16 noiseResHQ = 128, noiseResLQ = 32;

		ui::GUI &gui;
		FactoryContainer &factory;

		DescriptorsRef cameraDescriptor;
		GPUBufferRef seedBuffer, cameraRef, debugBuffer;

		DescriptorsRef descriptors;
		PipelineRef shader;
		PipelineLayoutRef shaderLayout;
		SamplerRef nearestSampler;

		SceneGraph *sceneGraph;
		Seed *seed;
		ui::StructInspector<DebugData*> debData;

		oic::Random r;

	public:

		CompositeTask(const GPUBufferRef &cam, FactoryContainer &factory, ui::GUI &gui);
		~CompositeTask();

		void prepareCommandList(CommandList *cl) override;

		void update(f64 dt) override;
		void resize(const Vec2u32 &size) override;
		void switchToScene(SceneGraph *sceneGraph) override;

	};

}