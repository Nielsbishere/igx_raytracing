#include "rt/task/main_task.hpp"
#include "rt/main.hpp"
#include "graphics/command/commands.hpp"
#include "graphics/enums.hpp"
#include "graphics/graphics.hpp"
#include "system/local_file_system.hpp"
#include "gui/window.hpp"
#include "input/keyboard.hpp"
#include "input/mouse.hpp"
#include "igxi/convert.hpp"

using namespace igx::ui;
using namespace oic;

namespace igx::rt {

	RaytracingInterface::RaytracingInterface(Graphics& g) : 
		g(g), gui(g), factory(g), compositeTask(factory, gui),
		sceneGraph(factory)
	{
		compositeTask.switchToScene(&sceneGraph);
		compositeTask.prepareMode(RenderMode::MQ);

		//TODO: If kS == 0, there won't be a reflection
		//		If kD == 0, there won't be shadow rays

		//Reserve command list

		cl = { g, NAME("Command list"), CommandList::Info(64_KiB) };
	}

	RaytracingInterface::~RaytracingInterface() { }

	//Create viewport resources

	void RaytracingInterface::init(ViewportInfo* vp) {

		if (swapchain.exists())
			oic::System::log()->fatal("Currently only supporting 1 viewport");

		//Create window swapchain

		g.resume();		//This thread can now interact with graphics

		swapchain = {
			g, NAME("Swapchain"),
			Swapchain::Info{ vp, false }
		};
	}

	//Delete viewport resources

	void RaytracingInterface::release(const ViewportInfo*) {
		swapchain.release();
	}

	//Update size of surfaces

	void RaytracingInterface::resize(const ViewportInfo*, const Vec2u32& size) {
		g.wait();
		gui.resize(size);
		compositeTask.resize(size);
	}

	//Execute commandList

	void RaytracingInterface::onRenderFinish(UploadBuffer *result, const Pair<u64, u64> &allocation, TextureObject *image, const Vec3u16&, const Vec3u16 &dim, u16, u8, bool) {

		//Convert to IGXI representation

		igxi::IGXI igxi{};

		igxi.header.width = dim.x;
		igxi.header.height = dim.y;
		igxi.header.length = dim.z;
		igxi.header.layers = igxi.header.mips = igxi.header.formats = 1;

		igxi.header.flags = igxi::IGXI::Flags::CONTAINS_DATA;
		igxi.header.type = TextureType::TEXTURE_2D;

		igxi.format.push_back(image->getInfo().format);
		igxi.data.push_back({ result->readback(allocation, image->size()) });

		igxi::Helper::toDiskExternal(igxi, targetOutput);
	}

	void RaytracingInterface::render(const ViewportInfo *vi) {

		if (compositeTask.needsCommandUpdate()) {
			cl->clear();
			sceneGraph.fillCommandList(cl);
			compositeTask.prepareCommandList(cl);
		}

		if (shouldOutputNextFrame) {

			//Setup render

			isResizeRequested = true;
			resize(vi, targetSize.cast<Vec2u32>());

			compositeTask.prepareMode(RenderMode::UQ);

			UploadBufferRef cpuOutput {
				g, "Frame output",
				UploadBuffer::Info(
					compositeTask.getTexture()->size(), 0, 0
				)
			};

			g.presentToCpu<RaytracingInterface, &RaytracingInterface::onRenderFinish>(
				{ cl }, compositeTask.getTexture(), cpuOutput, this
			);

			//Reset to old state and wait for work to finish

			Vec2u16 actualSize = swapchain->getInfo().size;
			resize(vi, Vec2u32(actualSize.x, actualSize.y));

			compositeTask.prepareMode(RenderMode::MQ);

			isResizeRequested = false;
			shouldOutputNextFrame = false;
		}

		//Regular render

		gui.render(g, vi->offset, vi->monitors);
		g.present(compositeTask.getTexture(), 0, 0, swapchain, gui.getCommands(), cl);
	}

	//Update eye
	void RaytracingInterface::update(const ViewportInfo *vi, f64 dt) {

		if (frameTime >= 1) {
			fps = frames / frameTime;
			frameTime = 0;
			frames = 0;
		}

		++frames;
		frameTime += dt;

		for(InputDevice *dev : vi->devices)
			if(dynamic_cast<Keyboard*>(dev))
				if(dev->isDown(Key::Key_ctrl))
					dt *= 1.5;

		sceneGraph.update(dt);
		compositeTask.update(dt);
	}

	//Input

	void RaytracingInterface::onInputUpdate(ViewportInfo*, const InputDevice *dvc, InputHandle ih, bool isActive) {

		//TODO: If input event "isActive" was triggered in compositeTask, then that one should keep track of it, vice versa for gui

		gui.onInputUpdate(dvc, ih, isActive);
	}
};

//Create window and wait for exit

int main() {

	ignis::Graphics g("Igx raytracing test", 1, "Igx", 1);
	igx::rt::RaytracingInterface viewportInterface(g);

	g.pause();

	System::viewportManager()->create(
		ViewportInfo(
			g.appName, {}, {}, 0, &viewportInterface, ViewportInfo::HANDLE_INPUT
		)
	);

	System::viewportManager()->waitForExit();

	return 0;
}