#include "rt/task/composite_task.hpp"
#include "rt/main.hpp"
#include "graphics/command/commands.hpp"
#include "graphics/enums.hpp"
#include "graphics/graphics.hpp"
#include "system/local_file_system.hpp"
#include "gui/window.hpp"
#include "input/keyboard.hpp"
#include "input/mouse.hpp"
#include "igxi/convert.hpp"
#include "rt/enums.hpp"

using namespace igx::ui;
using namespace oic;

namespace igx::rt {

	RaytracingInterface::RaytracingInterface(Graphics& g) : 
		g(g), gui(g), factory(g), 
		cameraBuffer{
			g, NAME("Camera buffer"),
			GPUBuffer::Info(
				sizeof(Camera), GPUBufferType::UNIFORM, GPUMemoryUsage::CPU_WRITE
			)
		}, 
		compositeTask(cameraBuffer, factory, gui),
		sceneGraph(factory)
	{
		compositeTask.switchToScene(&sceneGraph);
		compositeTask.prepareMode(RenderMode::MQ);

		//TODO: If kS == 0, there won't be a reflection
		//		If kD == 0, there won't be shadow rays

		//Reserve command list

		cl = { g, NAME("Command list"), CommandList::Info(64_KiB) };

		gui.addWindow(
			Window(
				"Main window", EDITOR_MAIN, {}, Vec2f32(360, 200),
				&properties, Window::DEFAULT_SCROLL_NO_CLOSE
			)
		);

		gui.addWindow(
			ui::Window(
				"Camera editor", EDITOR_CAMERA, 
				Vec2f32(0, 200), Vec2f32(360, 300),
				&cameraInspector, ui::Window::DEFAULT_SCROLL_NO_CLOSE
			)
		);
	}

	RaytracingInterface::~RaytracingInterface() { 
		gui.removeWindow(EDITOR_MAIN);
		gui.removeWindow(EDITOR_CAMERA);
	}

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

	void RaytracingInterface::resize(const ViewportInfo *vp, const Vec2u32& size) {

		g.wait();
		cl->clear();

		CPUCamera &camera = cameraInspector;

		camera.width = size.x; 
		camera.height = size.y;
		camera.invRes = Vec2f32(1.f / size.x, 1.f / size.y);

		camera.tiles = size / THREADS_XY;

		if(vp)
			swapchain->onResize(size);

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

		igxi::Helper::toDiskExternal(igxi, properties.value.targetOutput);
	}

	void RaytracingInterface::render(const ViewportInfo *vi) {

		if (properties.value.shouldOutputNextFrame) {

			//Setup render

			isResizeRequested = true;
			resize(nullptr, properties.value.getRes().cast<Vec2u32>());

			bool ui = bool(cameraInspector.value.flags & CameraFlags::USE_UI);
			cameraInspector.value.flags &= ~CameraFlags::USE_UI;

			if(properties.value.targetSamples > 1)
				cameraInspector.value.flags |= CameraFlags::USE_SUPERSAMPLING;

			update(vi, 0);

			cl->add(FlushBuffer(cameraBuffer, factory.getDefaultUploadBuffer()));
			sceneGraph.fillCommandList(cl);
			compositeTask.prepareCommandList(cl);

			compositeTask.prepareMode(RenderMode::UQ);

			UploadBufferRef cpuOutput {
				g, "Frame output",
				UploadBuffer::Info(
					compositeTask.getTexture()->size(), 0, 0
				)
			};

			List<CommandList*> cls(properties.value.targetSamples, cl);

			g.presentToCpu<RaytracingInterface, &RaytracingInterface::onRenderFinish>(
				cls, compositeTask.getTexture(), cpuOutput, this
			);

			//Reset to old state and wait for work to finish

			Vec2u16 actualSize = swapchain->getInfo().size;
			resize(nullptr, Vec2u32(actualSize.x, actualSize.y));

			compositeTask.prepareMode(RenderMode::MQ);

			isResizeRequested = false;
			properties.value.shouldOutputNextFrame = false;

			if(ui)
				cameraInspector.value.flags |= CameraFlags::USE_UI;

			cameraInspector.value.flags &= ~CameraFlags::USE_SUPERSAMPLING;
		}

		//Regular render

		if (compositeTask.needsCommandUpdate()) {
			cl->clear();
			cl->add(FlushBuffer(cameraBuffer, factory.getDefaultUploadBuffer()));
			sceneGraph.fillCommandList(cl);
			compositeTask.prepareCommandList(cl);
		}
		
		if (!bool(cameraInspector.value.flags & CameraFlags::USE_UI)) {
			g.present(compositeTask.getTexture(), 0, 0, swapchain, cl);
			return;
		}

		gui.render(g, vi->offset, vi->monitors);
		g.present(compositeTask.getTexture(), 0, 0, swapchain, gui.getCommands(), cl);
	}

	//Update eye
	void RaytracingInterface::update(const ViewportInfo *vi, f64 dt) {

		if (frameTime >= 1) {
			properties.value.fps = frames / frameTime;
			frameTime = 0;
			frames = 0;
		}

		++frames;
		frameTime += dt;

		f32 speedUp = 1;

		for(InputDevice *dev : vi->devices)
			if (dynamic_cast<Keyboard*>(dev)) {

				if (dev->isDown(Key::Key_ctrl))
					dt *= 1.5;

				if (dev->isDown(Key::Key_shift))
					speedUp *= 2;
			}

		CPUCamera &camera = cameraInspector;

		auto v = camera.getView(0);

		Vec3f32 d = dir.clamp(-1, 1) * f32(dt * camera.speed * speedUp);
		camera.eye += (v * Vec4f32(d.x, d.y, d.z, 0)).cast<Vec3f32>();

		bool isStereo =
			camera.projectionType == ProjectionType::Stereoscopic_TB ||
			camera.projectionType == ProjectionType::Stereoscopic_LR;

		auto vLeft = camera.getView(isStereo ? -1.f : 0);

		if (
			camera.projectionType != ProjectionType::Omnidirectional && 
			camera.projectionType != ProjectionType::Stereoscopic_omnidirectional_LR && 
			camera.projectionType != ProjectionType::Stereoscopic_omnidirectional_TB
		) {

			Vec2f32 res(f32(camera.width), f32(camera.height));

			if (isStereo) {

				if (camera.projectionType == ProjectionType::Stereoscopic_LR)
					res.x /= 2;

				else res.y /= 2;
			}

			const f32 aspect = res.aspect();
			const f32 nearPlaneLeft = f32(std::tan(camera.leftFov * 0.5_deg));

			camera.p0 = (vLeft * Vec4f32(-aspect, 1, -nearPlaneLeft, 1)).cast<Vec3f32>();
			camera.p1 = (vLeft * Vec4f32(aspect, 1, -nearPlaneLeft, 1)).cast<Vec3f32>();
			camera.p2 = (vLeft * Vec4f32(-aspect, -1, -nearPlaneLeft, 1)).cast<Vec3f32>();

			if (isStereo) {

				auto vRight = camera.getView(1);
				const f32 nearPlaneRight = f32(std::tan(camera.rightFov * 0.5_deg));

				camera.p3 = (vRight * Vec4f32(-aspect, 1, -nearPlaneRight, 1)).cast<Vec3f32>();
				camera.p4 = (vRight * Vec4f32(aspect, 1, -nearPlaneRight, 1)).cast<Vec3f32>();
				camera.p5 = (vRight * Vec4f32(-aspect, -1, -nearPlaneRight, 1)).cast<Vec3f32>();

			}
		}

		std::memcpy(cameraBuffer->getBuffer(), &camera, sizeof(Camera));
		cameraBuffer->flush(0, sizeof(Camera));

		sceneGraph.update(dt);
		compositeTask.update(dt);
	}

	//Input

	void RaytracingInterface::onInputUpdate(ViewportInfo*, const InputDevice *dvc, InputHandle ih, bool isActive) {

		if (ih == Key::Key_f1 && !isActive)
			cameraInspector.value.flags ^= CameraFlags::USE_UI;

		bool hasUpdated = gui.onInputUpdate(dvc, ih, isActive);

		if (dynamic_cast<const Keyboard*>(dvc)) {

			f32 v = isActive ? 1.f : -1.f;

			static constexpr Key keys[] = {
				Key::Key_a, Key::Key_d,			//x
				Key::Key_q, Key::Key_e,			//y
				Key::Key_w, Key::Key_s			//z
			};

			constexpr usz j = _countof(keys);
			usz i = 0;

			for (; i < j; ++i)
				if (keys[i].value == ih)
					break;

			if (i == j)
				return;

			usz i2 = i >> 1;

			if (!isActive && !dir.arr[i2])
				return;

			if (isActive && hasUpdated)
				return;

			dir.arr[i2] += i & 1 ? v : -v;
		}
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