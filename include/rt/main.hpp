#pragma once
#include "task/composite_task.hpp"
#include "helpers/factory.hpp"
#include "system/viewport_interface.hpp"
#include "gui/gui.hpp"
#include "gui/struct_inspector.hpp"
#include "gui/ui_value.hpp"
#include "types/mat.hpp"
#include "utils/random.hpp"
#include "utils/inflect.hpp"
#include "scene/niels_scene.hpp"

#include "../res/shaders/defines.glsl"

namespace igx::rt {

	oicExposedEnum(
		Resolution, u8,
		CUSTOM,
		SD,
		HD,
		FHD,
		QHD,
		UHD_4K,
		UHD_8K
	);

	static constexpr Vec2u16 pixelsByResolution[] = {
		{},
		{ 720, 480 },
		{ 1280, 720 },
		{ 1920, 1080 },
		{ 2560, 1440 },
		{ 3840, 2160 },
		{ 7680, 4320 }
	};
	
	struct RaytracingProperties {

		//Outputting to image

		String targetOutput = "./output/0";

		Vec2u16 targetSize = { 7680, 4320 };

		f64 fps{};

		ui::Slider<u16, 1, 64> targetSamples = 16;

		Resolution res = Resolution::UHD_8K;

		bool shouldOutputNextFrame{}, isPortrait{};

		inline void exportToPNG() const {		//TODO: Non const!
			(bool&) shouldOutputNextFrame = true;
		}

		InflectBody(

			static const List<String> memberNames = {
				"FPS", "Output path",
				"Samples per pixel", "Output resolution preset", "Output size",
				"Use portrait mode",
				"Export to PNG"
			};

			if(res == Resolution::CUSTOM)
				inflector.inflect(
					this, recursion, memberNames, 
					(const f64&) fps, targetOutput, targetSamples, res, targetSize, isPortrait,
					Button<RaytracingProperties, &RaytracingProperties::exportToPNG>{}
				);

			else inflector.inflect(
				this, recursion, memberNames, 
				(const f64&) fps, targetOutput, targetSamples, res, (const Vec2u16&) targetSize, isPortrait,
				Button<RaytracingProperties, &RaytracingProperties::exportToPNG>{}
			);

			if constexpr (!std::is_const_v<decltype(*this)>)
				if (res != Resolution::CUSTOM)
					targetSize = pixelsByResolution[res.value];

		);

		inline Vec2u16 getRes() const {
			return isPortrait ? targetSize.swap() : targetSize;
		}

	};

	class RaytracingInterface : public oic::ViewportInterface {
	
		Graphics& g;

		ui::GUI gui;

		GPUBufferRef cameraBuffer;

		FactoryContainer factory;
		CompositeTask compositeTask;
	
		SwapchainRef swapchain;
		CommandListRef cl;
		NielsScene sceneGraph;

		oic::Random r;

		ui::StructInspector<RaytracingProperties> properties;
		ui::StructInspector<CPUCamera> cameraInspector;

		Vec3f32 dir;

		f64 frameTime{};
		u32 frames{};

		bool isResizeRequested{};

	public:
	
		//Functions
	
		RaytracingInterface(Graphics &g);
		~RaytracingInterface();
	
		void init(oic::ViewportInfo *vp) final override;
	
		void release(const oic::ViewportInfo*) final override;
	
		void resize(const oic::ViewportInfo*, const Vec2u32& size) final override;
	
		void onRenderFinish(UploadBuffer*, const Pair<u64, u64>&, TextureObject*, const Vec3u16&, const Vec3u16&, u16, u8, bool);
	
		void render(const oic::ViewportInfo*) final override;
		void update(const oic::ViewportInfo*, f64) final override;
		void onInputUpdate(oic::ViewportInfo*, const oic::InputDevice*, oic::InputHandle, bool) final override;
	};

}