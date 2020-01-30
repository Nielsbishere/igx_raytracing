#include "graphics/command/commands.hpp"
#include "helpers/graphics_object_ref.hpp"
#include "graphics/enums.hpp"
#include "graphics/graphics.hpp"
#include "system/viewport_manager.hpp"
#include "system/viewport_interface.hpp"
#include "system/local_file_system.hpp"
#include "gui/gui.hpp"
#include "gui/window.hpp"
#include "types/mat.hpp"
#include "input/keyboard.hpp"
#include "input/mouse.hpp"
#include "utils/math.hpp"

using namespace igx::ui;
using namespace igx;
using namespace oic;

struct RaytracingInterface : public ViewportInterface {

	Graphics& g;

	//Resources

	Swapchain swapchain;
	CommandList cl;
	PrimitiveBuffer mesh;
	Descriptors raygenDescriptors;
	Pipeline raygenPipeline;
	ShaderBuffer raygenData, sphereData, materialData, planeData, triangleData;
	Texture raytracingOutput, tex2D;
	Sampler samp;

	Vec2u32 res;
	Vec3f32 eye{ 0, 0, 7 };
	f64 speed = 2, fovChangeSpeed = 7;
	f32 fov = f32(70_deg);

	//Data required for raygen
	struct RaygenData {
		Vec4f32 eye, p0, p1, p2;
	};

	//Material data
	struct Material {

		Vec4f32 colorSpecular;

		Vec4f32 ambientIor;

		Vec4f32 emissiveRoughness;

		f32 metallic;
		f32 transparency;
		u32 materialInfo;
		u32 padding;
	};

	//Triangle data

	struct Triangle {

		Vec3f32 p0;
		u32 object;

		Vec3f32 p1;
		u32 material;

		Vec3f32 p2;
		u32 padding;
	};

	//Create resources

	RaytracingInterface(Graphics& g) : g(g) {

		//Create primitive buffer

		List<BufferAttributes> attrib{ { 0, GPUFormat::RGB32f }, { 1, GPUFormat::RG32f } };

		const List<Vec3f32> positionBuffer{

			//Bottom
			{ -1, -1, -1 }, { 1, -1, -1 },
			{ 1, -1, 1 },	{ -1, -1, 1 },

			//Top
			{ -1, 1, -1 },  { 1, 1, -1 },
			{ 1, 1, 1 },	{ -1, 1, 1 },

			//Back
			{ -1, -1, -1 }, { -1, 1, -1 },
			{ 1, 1, -1 },	{ 1, -1, -1 },

			//Front
			{ -1, -1, 1 },  { -1, 1, 1 },
			{ 1, 1, 1 },	{ 1, -1, 1 },

			//Left
			{ -1, -1, -1 }, { -1, -1, 1 },
			{ -1, 1, 1 },	{ -1, 1, -1 },

			//Right
			{ 1, -1, -1 },  { 1, -1, 1 },
			{ 1, 1, 1 },	{ 1, 1, -1 },
		};

		const List<Vec2f32> uvBuffer{

			//Bottom
			{ 0, 0 }, { 1, 0 },
			{ 1, 1 }, { 0, 1 },

			//Top
			{ 0, 1 }, { 1, 1 },
			{ 1, 0 }, { 0, 0 },

			//Back
			{ 0, 0 }, { 1, 0 },
			{ 1, 1 }, { 0, 1 },

			//Front
			{ 0, 1 }, { 1, 1 },
			{ 1, 0 }, { 0, 0 },

			//Left
			{ 0, 0 }, { 1, 0 },
			{ 1, 1 }, { 0, 1 },

			//Right
			{ 0, 1 }, { 1, 1 },
			{ 1, 0 }, { 0, 0 }
		};

		const List<u8> iboBuffer{
			0,3,2, 2,1,0,			//Bottom
			4,5,6, 6,7,4,			//Top
			8,11,10, 10,9,8,		//Back
			12,13,14, 14,15,12,		//Front
			16,19,18, 18,17,16,		//Left
			20,21,22, 22,23,20		//Right
		};

		mesh = {
			g, NAME("Test mesh"),
			PrimitiveBuffer::Info(
				{
					BufferLayout(positionBuffer, attrib[0]),
					BufferLayout(uvBuffer, attrib[1])
				},
				BufferLayout(iboBuffer, { 0, GPUFormat::R8u })
			)
		};

		//Create uniform buffer

		raygenData = {
			g, NAME("Raygen uniform buffer"),
			ShaderBuffer::Info(
				GPUBufferType::UNIFORM, GPUMemoryUsage::CPU_WRITE,
				{ { NAME("mask"), ShaderBufferLayout(0, Buffer(sizeof(RaygenData))) } }
			)
		};

		Vec4f32 spheres[] = {
			Vec4f32(0, 0, 5, 1),
			Vec4f32(0, 0, -5, 1),
			Vec4f32(0, 5, 0, 1),
			Vec4f32(0, -5, 0, 1),
			Vec4f32(5, 0, 0, 1),
			Vec4f32(-5, 0, 0, 1)
		};

		sphereData = {
			g, NAME("Sphere data"),
			ShaderBuffer::Info(
				GPUBufferType::STRUCTURED, GPUMemoryUsage::LOCAL,
				{ { NAME("spheres"), ShaderBufferLayout(0, Buffer((u8*)spheres, (u8*)spheres + sizeof(spheres))) } }
			)
		};

		Vec4f32 planes[] = {
			Vec4f32(0, 1, 0, -5)
		};

		planeData = {
			g, NAME("Plane data"),
			ShaderBuffer::Info(
				GPUBufferType::STRUCTURED, GPUMemoryUsage::LOCAL,
				{ { NAME("planes"), ShaderBufferLayout(0, Buffer((u8*)planes, (u8*)planes + sizeof(planes))) } }
			)
		};

		Triangle triangles[] = {
			{ Vec3f32(-1, 1, 0), 0, Vec3f32(1, 1, 0), 0, Vec3f32(1, -1, 0), 0 },
			{ Vec3f32(-1, 4, 0), 0, Vec3f32(1, 4, 0), 1, Vec3f32(1, 3, 0), 0 },
			{ Vec3f32(-1, 7, 0), 0, Vec3f32(1, 7, 0), 2, Vec3f32(1, 5, 0), 0 }
		};

		triangleData = {
			g, NAME("Triangle data"),
			ShaderBuffer::Info(
				GPUBufferType::STRUCTURED, GPUMemoryUsage::LOCAL,
				{ { NAME("triangles"), ShaderBufferLayout(0, Buffer((u8*)triangles, (u8*)triangles + sizeof(triangles))) } }
			)
		};

		Material materials[] = {
			{ { 1, 0, 0, 0 }, { 0.01f, 0, 0, 0 }, { 0, 0, 0, 1 }, 0, 0, 0, 0 },
			{ { 0, 1, 0, 0 }, { 0, 0.01f, 0, 0 }, { 0, 0, 0, 1 }, 0, 0, 0, 0 },
			{ { 0, 0, 1, 0 }, { 0, 0, 0.01f, 0 }, { 0, 0, 0, 1 }, 0, 0, 0, 0 },
			{ { 1, 0, 1, 0 }, { 0.01f, 0, 0.01f, 0 }, { 0, 0, 0, 1 }, 0, 0, 0, 0 },
			{ { 1, 1, 0, 0 }, { 0.01f, 0.01f, 0, 0 }, { 0, 0, 0, 1 }, 0, 0, 0, 0 },
			{ { 0, 1, 1, 0 }, { 0, 0.01f, 0.01f, 0 }, { 0, 0, 0, 1 }, 0, 0, 0, 0 }
		};

		materialData = {
			g, NAME("Material data"),
			ShaderBuffer::Info(
				GPUBufferType::STRUCTURED, GPUMemoryUsage::LOCAL,
				{ { NAME("materials"), ShaderBufferLayout(0, Buffer((u8*)materials, (u8*)materials + sizeof(materials))) } }
			)
		};

		//Create texture

		const u32 rgba0[4][2] = {

			{ 0xFFFF00FF, 0xFF00FFFF },	//1,0,1, 0,1,1
			{ 0xFFFFFF00, 0xFFFFFFFF },	//1,1,0, 1,1,1

			{ 0xFF7F007F, 0xFF7F0000 },	//.5,0,.5, .5,0,0
			{ 0xFF7F7F7F, 0xFF007F00 }	//.5,.5,.5, 0,.5,0
		};

		const u32 rgba1[2][1] = {
			{ 0xFFBFBFBF },				//0.75,0.75,0.75
			{ 0xFF5F7F7F }				//0.375,0.5,0.5
		};

		tex2D = {
			g, NAME("Test texture"),
			Texture::Info(
				List<Grid2D<u32>>{
					rgba0, rgba1
				},
				GPUFormat::RGBA8, GPUMemoryUsage::LOCAL, 2
			)
		};

		//Create sampler

		samp = { g, NAME("Test sampler"), Sampler::Info() };

		//Load shader code

		Buffer raygenComp;
		oicAssert("Couldn't load raygen shader", oic::System::files()->read("./shaders/raygen.comp.spv", raygenComp));

		//Create layout for compute

		PipelineLayout pipelineLayout(
			RegisterLayout(NAME("Output"), 0, TextureType::TEXTURE_2D, 0, ShaderAccess::COMPUTE, true),
			RegisterLayout(NAME("Raygen data"), 1, GPUBufferType::UNIFORM, 0, ShaderAccess::COMPUTE, sizeof(RaygenData)),
			RegisterLayout(NAME("Spheres"), 2, GPUBufferType::STRUCTURED, 0, ShaderAccess::COMPUTE, sizeof(Vec4f32)),
			RegisterLayout(NAME("Materials"), 3, GPUBufferType::STRUCTURED, 1, ShaderAccess::COMPUTE, sizeof(Material)),
			RegisterLayout(NAME("Planes"), 4, GPUBufferType::STRUCTURED, 2, ShaderAccess::COMPUTE, sizeof(Vec4f32)),
			RegisterLayout(NAME("Triangles"), 5, GPUBufferType::STRUCTURED, 3, ShaderAccess::COMPUTE, sizeof(Triangle))
		);

		auto descriptorsInfo = Descriptors::Info(pipelineLayout, {});
		descriptorsInfo.resources[0] = nullptr;
		descriptorsInfo.resources[1] = { raygenData, 0 };
		descriptorsInfo.resources[2] = { sphereData, 0 };
		descriptorsInfo.resources[3] = { materialData, 0 };
		descriptorsInfo.resources[4] = { planeData, 0 };
		descriptorsInfo.resources[5] = { triangleData, 0 };

		raygenDescriptors = {
			g, NAME("Raygen descriptors"),
			descriptorsInfo
		};

		//Create pipelines

		raygenPipeline = {
			g, NAME("Raygen pipeline"),
			Pipeline::Info(
				PipelineFlag::OPTIMIZE,
				raygenComp,
				pipelineLayout,
				Vec3u32{ 8, 8, 1 }
			)
		};

		//Release the graphics instance for us until we need it again

		g.pause();
	}

	//Create viewport resources

	void init(ViewportInfo* vp) final override {

		if (swapchain.exists())
			oic::System::log()->fatal("Currently only supporting 1 viewport");

		//Create window swapchain

		swapchain = {
			g, NAME("Swapchain"),
			Swapchain::Info{ vp, false }
		};
	}

	//Delete viewport resources

	void release(const ViewportInfo*) final override {
		swapchain.release();
	}

	//Helper functions

	void updateUniforms() {

		auto v = Mat4x4f32::lookDirection(eye, { 0, 0, -1 }, { 0, 1, 0 });

		const f32 aspect = res.cast<Vec2f32>().aspect();

		Vec4f32 eye4 = (-eye).cast<Vec4f32>();
		eye4.w = 1;

		const Vec4f32 p0 = v * Vec4f32(-aspect, 1, -1, 1);
		const Vec4f32 p1 = v * Vec4f32(aspect, 1, -1, 1);
		const Vec4f32 p2 = v * Vec4f32(-aspect, -1, -1, 1);

		RaygenData buffer {
			eye4,
			p0,
			p1,
			p2
		};

		memcpy(raygenData->getBuffer(), &buffer, sizeof(buffer));
		raygenData->flush(0, sizeof(buffer));
	}

	//Update size of surfaces

	void resize(const ViewportInfo*, const Vec2u32& size) final override {

		res = size;
		updateUniforms();
		
		//Update compute target

		raytracingOutput.release();
		raytracingOutput = {
			g, NAME("Raytracing output"),
			Texture::Info(
				size.cast<Vec2u16>(), GPUFormat::RGBA16f, GPUMemoryUsage::GPU_WRITE, 1, 1
			)
		};

		raygenDescriptors->updateDescriptor(0, { raytracingOutput, TextureType::TEXTURE_2D });
		raygenDescriptors->flush(0, 1);
		
		//Create command list and store our commands

		cl.release();
		cl = { g, NAME("Command list"), CommandList::Info(1_KiB) };

		cl->add(
			BindPipeline(raygenPipeline),
			BindDescriptors(raygenDescriptors),
			Dispatch(size.cast<Vec2u32>())
		);

		swapchain->onResize(size);
	}

	//Execute commandList

	void render(const ViewportInfo*) final override {
		g.present(raytracingOutput, swapchain, cl);
	}

	//Update eye
	void update(const ViewportInfo* vi, f64 dt) final override {

		Vec3f32 d{};

		for (auto* dvc : vi->devices)
			if (dvc->isType(InputDevice::KEYBOARD)) {

				if (dvc->isDown(Key::KEY_W)) d += Vec3f32(0, 0, -1);
				if (dvc->isDown(Key::KEY_S)) d += Vec3f32(0, 0, 1);

				if (dvc->isDown(Key::KEY_Q)) d += Vec3f32(0, -1, 0);
				if (dvc->isDown(Key::KEY_E)) d += Vec3f32(0, 1, 0);

				if (dvc->isDown(Key::KEY_D)) d += Vec3f32(1, 0, 0);
				if (dvc->isDown(Key::KEY_A)) d += Vec3f32(-1, 0, 0);

			}
			else if (dvc->isType(InputDevice::MOUSE)) {

				f64 delta = dvc->getCurrentAxis(MouseAxis::AXIS_WHEEL);

				if (delta)
					speed = oic::Math::clamp(speed * 1 + (delta / 1024), 0.5, 5.0);
			}

		if (d.any())
			eye += d.clamp(-1, 1) * f32(dt * speed);

		if (res.neq(0).all())
			updateUniforms();
	}
};

//Create window and wait for exit

int main() {

	Graphics g;
	RaytracingInterface viewportInterface(g);

	g.pause();

	System::viewportManager()->create(
		ViewportInfo(
			"Raytracer", {}, {}, 0, &viewportInterface, ViewportInfo::HANDLE_INPUT
		)
	);

	//TODO: Better way of stalling than this; like interrupt
	while (System::viewportManager()->size())
		System::wait(250_ms);

	return 0;
}