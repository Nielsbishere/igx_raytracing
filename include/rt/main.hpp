#pragma once
#include "task/main_task.hpp"
#include "helpers/factory.hpp"
#include "system/viewport_interface.hpp"
#include "gui/gui.hpp"
#include "gui/struct_inspector.hpp"
#include "types/mat.hpp"
#include "utils/random.hpp"
#include "utils/inflect.hpp"
#include "scene/niels_scene.hpp"

#include "../res/shaders/defines.glsl"

namespace igx::rt {
	
	struct RaytracingInterface : public oic::ViewportInterface {
	
		Graphics& g;

		FactoryContainer factory;
		CompositeTask compositeTask;
	
		igx::ui::GUI gui;
	
		SwapchainRef swapchain;
		CommandListRef cl;
		NielsScene sceneGraph;

		oic::Random r;

		//Outputting to image

		String targetOutput = "./output/0";

		Vec2u16 targetSize = { 7680, 4320 };

		u16 targetSamples = 64;

		bool shouldOutputNextFrame{}, isResizeRequested{};

		//Track fps
	
		f64 frameTime{}, fps{};
		u32 frames{};
	
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