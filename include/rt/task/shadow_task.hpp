#pragma once
#include "helpers/render_task.hpp"
#include "helpers/factory.hpp"
#include "gui/struct_inspector.hpp"
#include "gui/ui_value.hpp"
#include "utils/random.hpp"

namespace igx::rt {

	class RaygenTask;

	struct ShadowProperties {

		ui::Slider<u32, 1, 128> Shadow_samples = 1;

		Inflect(Shadow_samples);

	};

	class ShadowTask : public TextureRenderTask {

		oic::Random random;

		FactoryContainer &factory;

		SceneGraph *sceneGraph;
		RaygenTask *raygen;

		GPUBufferRef shadowProperties, shadowOutput, seed;
		SamplerRef nearestSampler, nearestWrapSampler;

		TextureRef blueNoise;

		PipelineRef shadowShader, lightingShader;
		PipelineLayoutRef shadowLayout, lightingLayout;

		DescriptorsRef shadowDescriptors, lightingDescriptors, cameraDescriptor;

		ui::StructInspector<ShadowProperties> properties;

		u32 cachedSamples{};

	public:

		ShadowTask(
			FactoryContainer &factory,
			RaygenTask *raygen,
			const TextureRef &blueNoise,
			const GPUBufferRef &seed,
			const DescriptorsRef &cameraDescriptor
		);

		bool needsCommandUpdate() const;
		void prepareCommandList(CommandList *cl) override;

		void update(f64) override;
		void resize(const Vec2u32 &size) override;

		void switchToScene(SceneGraph *sceneGraph) override;
	};

}