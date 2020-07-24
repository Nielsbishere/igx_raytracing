#include "graphics/command/commands.hpp"
#include "helpers/common.hpp"
#include "graphics/enums.hpp"
#include "graphics/graphics.hpp"
#include "system/viewport_manager.hpp"
#include "system/viewport_interface.hpp"
#include "system/local_file_system.hpp"
#include "gui/gui.hpp"
#include "gui/struct_inspector.hpp"
#include "gui/window.hpp"
#include "types/mat.hpp"
#include "input/keyboard.hpp"
#include "input/mouse.hpp"
#include "utils/math.hpp"
#include "utils/random.hpp"
#include "utils/inflect.hpp"

#define uint u32
#include "../res/shaders/defines.glsl"
#undef uint

using namespace igx::ui;
using namespace igx;
using namespace oic;

oicExposedEnum(LightType, u16, Directional, Point, Spot);
oicExposedEnum(
	DisplayType, u32, 
	Default, Accumulation,
	Intersection_attributes, Normals, Albedo,
	Light_buffer, Reflection_buffer, No_secondaries,
	UI_Only, Material, Object,
	Intersection_side
);

struct RaytracingInterface : public ViewportInterface {

	Graphics& g;

	//Resources

	GUI gui;

	SwapchainRef swapchain;
	CommandListRef cl;
	PrimitiveBufferRef mesh;
	DescriptorsRef raytracingDescriptors;
	PipelineRef raygenPipeline, dispatchSetup, shadowPipeline, postProcessing, reflectionPipeline;
	TextureRef raytracingOutput, raytracingAccumulation, reflectionBuffer, tex2D;
	SamplerRef samp;

	ShaderBufferRef 
		raygenData, sphereData, planeData, triangleData, lightData,
		materialData, shadowRayData, counterBuffer, dispatchArgs, positionBuffer,
		reflectionRayData, cubeData, shadowColors, shadowOutput, gbuffer;

	UploadBufferRef uploadBuffer;

	Vec2u32 res;
	Vec3f32 eye{ 0, 0, -7 }, eyeDir = { 0, 0, -1 };
	Mat4x4f32 v = Mat4x4f32::lookDirection(eye, eyeDir, { 0, 1, 0 });

	SamplerRef samplerNearest;

	f64 speed = 5, fovChangeSpeed = 7;
	f32 fov = 70;

	u32 sampleCount{};

	static constexpr usz sphereCount = 7;
	static constexpr usz triCount = 3;
	static constexpr usz cubeCount = 1;
	static constexpr usz planeCount = 1;

	oic::Random r;

	u8 pipelineUpdates{};

	String pipelines[5] = {
		"./shaders/raygen.comp.spv",
		"./shaders/dispatch_setup.comp.spv",
		"./shaders/shadow.comp.spv",
		"./shaders/post_processing.comp.spv",
		"./shaders/reflection.comp.spv"
	};

	//Data required for raygen
	struct GPUData {

		Vec3f32 eye;
		u32 width;
			
		Vec3f32 p0;
		u32 height;
		
		Vec3f32 p1;
		f32 randomX;
		
		Vec3f32 p2;
		f32 randomY;

		Vec3f32 skyboxColor; f32 exposure;

		Vec2f32 invRes;
		f32 focalDistance;
		f32 aperature;

		u32 sampleCount;
		u32 triCount;
		u32 sphereCount;
		u32 cubeCount;

		u32 planeCount;
		DisplayType displayType;

		InflectWithName(
			{
				"Eye",
				"Skybox color",
				"Exposure",
				"Test float",
				"Display time"
			},
			eye,
			skyboxColor,
			exposure,
			Vec3f32(1, 0, 0),
			displayType
		);

	};

	//A ray payload
	struct RayPayload {

		Vec2f32 dir;				//Vector direction in spherical coords
		u32 screenCoord;
		f32 maxDist;

		Vec2u32 color, padding;
	};

	//A shadow payload
	struct ShadowPayload {

		Vec3f32 pos;
		u32 loc1D;

		Vec3f32 dir;
		f32 maxDist;

	};

	//The counter buffer
	struct CounterBuffer {
		u32 shadowCounter;
		u32 reflractionCounter;
	};

	//Material data
	struct Material {

		Vec3f32 albedo;
		f32 metallic;

		Vec3f32 ambient;
		f32 roughness;

		Vec3f32 emissive;
		f32 transparency;

		u32 padding[3];
		u32 materialInfo;
	};

	/*/A light
	struct Light {

		Vec3f32 pos;
		f16 rad, ori;

		Vec4u16 rot;
		Vec3u16 col;			//TODO: Vec3f16
		LightType type;

		InflectBody(
			switch (type) {

				case LightType::Directional:
					inflector.inflect(this, recursion, { "Type", "Direction", "Color", "Angular extent (in degrees, sun is 0.53)" }, type, rot, col, ori);
					break;

				case LightType::Point:
					inflector.inflect(this, recursion, { "Type", "Position", "Radius", "Origin", "Color" }, type, pos, rad, ori, col);
					break;

				case LightType::Spot:
					inflector.inflect(this, recursion, { "Type", "Position", "Rotation", "Radius", "Origin", "Color" }, type, rot, pos, rad, ori, col);
					break;

			}
		);

	};*/

	struct Light {

		Vec3f32 pos;
		f32 rad;

		Vec3f32 dir;
		u32 type;

		Vec3f32 color;
		f32 angle;

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

	//Cubes

	struct Cube {

		Vec3f32 start;
		u32 object;

		Vec3f32 end;
		u32 material;

	};

	bool isFileListening{};

	static constexpr u16
		maxShadowRaysPerPixel = 6,		//Expect 6 shadow rays per pixel max
		avgShadowRaysPerPixel = 3;		//Expect 3 shadow rays per pixel avg

	struct Hit {

		Vec2f32 intersection;
		f32 hitT;
		u32 materialId;

		Vec3f32 nrm;
		u32 objectId;

	};

	//GPU data
	PipelineLayoutRef pipelineLayout;

	//

	GPUData *gpuData{};

	InflectWithName(
		{ "GPUData", "FOV", "FOV Change speed", "Eye dir", "Move speed", "View matrix", "Reset accumulation samples" }, 
		*gpuData, fov, fovChangeSpeed, eyeDir, speed, (const Mat4x4f32&) v
	);

	//UI
	StructInspector<RaytracingInterface*> inspector;

	//Create resources

	RaytracingInterface(Graphics& g) : g(g), gui(g), inspector(this) {

		gui.addWindow(Window("", 0, {}, { 200, 350 }, &inspector, Window::DEFAULT_SCROLL_NO_CLOSE));

		//Pipeline layout

		samplerNearest = {
			g, NAME("Nearest sampler"),
			Sampler::Info(
				SamplerMin::NEAREST, SamplerMag::NEAREST,
				SamplerMode::CLAMP_BORDER, 1
			)
		};

		pipelineLayout = {

			g, NAME("Raytracing pipeline layout"),

			PipelineLayout::Info(
				RegisterLayout(NAME("Output"), 0, TextureType::TEXTURE_2D, 0, ShaderAccess::COMPUTE, GPUFormat::RGBA16f, true),
				RegisterLayout(NAME("Raygen data"), 1, GPUBufferType::UNIFORM, 0, ShaderAccess::COMPUTE, sizeof(GPUData)),
				RegisterLayout(NAME("Spheres"), 2, GPUBufferType::STRUCTURED, 0, ShaderAccess::COMPUTE, sizeof(Vec4f32)),
				RegisterLayout(NAME("Planes"), 4, GPUBufferType::STRUCTURED, 1, ShaderAccess::COMPUTE, sizeof(Vec4f32)),
				RegisterLayout(NAME("Triangles"), 5, GPUBufferType::STRUCTURED, 2, ShaderAccess::COMPUTE, sizeof(Triangle)),
				RegisterLayout(NAME("Materials"), 3, GPUBufferType::STRUCTURED, 3, ShaderAccess::COMPUTE, sizeof(Material)),
				RegisterLayout(NAME("ShadowRays"), 6, GPUBufferType::STRUCTURED, 4, ShaderAccess::COMPUTE, sizeof(ShadowPayload)),
				RegisterLayout(NAME("CounterBuffer"), 7, GPUBufferType::STORAGE, 5, ShaderAccess::COMPUTE, sizeof(CounterBuffer)),
				RegisterLayout(NAME("DispatchArgs"), 8, GPUBufferType::STRUCTURED, 6, ShaderAccess::COMPUTE, sizeof(Vec4u32)),
				RegisterLayout(NAME("Lights"), 9, GPUBufferType::STRUCTURED, 7, ShaderAccess::COMPUTE, sizeof(Light)),
				RegisterLayout(NAME("PositionBuffer"), 10, GPUBufferType::STRUCTURED, 8, ShaderAccess::COMPUTE, sizeof(Vec4f32)),
				RegisterLayout(NAME("ReflectionRays"), 11, GPUBufferType::STRUCTURED, 9, ShaderAccess::COMPUTE, sizeof(RayPayload)),
				RegisterLayout(NAME("Cubes"), 12, GPUBufferType::STRUCTURED, 10, ShaderAccess::COMPUTE, sizeof(Cube)),
				RegisterLayout(NAME("Accumulation buffer"), 13, TextureType::TEXTURE_2D, 1, ShaderAccess::COMPUTE, GPUFormat::RGBA32f, true),
				RegisterLayout(NAME("Reflection buffer"), 14, TextureType::TEXTURE_2D, 2, ShaderAccess::COMPUTE, GPUFormat::RGBA16f, true),
				RegisterLayout(NAME("Shadow output"), 15, GPUBufferType::STRUCTURED, 11, ShaderAccess::COMPUTE, sizeof(u32)),
				RegisterLayout(NAME("Shadow colors"), 16, GPUBufferType::STRUCTURED, 12, ShaderAccess::COMPUTE, sizeof(Vec2u32)),
				RegisterLayout(NAME("HitBuffer"), 17, GPUBufferType::STRUCTURED, 13, ShaderAccess::COMPUTE, sizeof(Hit)),
				RegisterLayout(NAME("UI"), 18, SamplerType::SAMPLER_MS, 0, ShaderAccess::COMPUTE)
			)
		};

		//Upload buffer

		uploadBuffer = {
			g, NAME("Upload buffer"),
			UploadBuffer::Info(1_MiB, 1_MiB, 4_MiB)
		};

		/* TODO: Shader reloading

		#ifndef NDEBUG

		oic::System::files()->addFileChangeCallback(
			[](FileSystem *, const FileInfo &inf, FileChange cng, void *intr) -> void {
		
				if (cng == FileChange::UPDATE) {

					auto *rti = (RaytracingInterface*)intr;

					usz i{};

					for (auto &path : rti->pipelines) {

						if (inf.path == path) {
							rti->pipelineUpdates |= 1 << i;
							break;
						}

						++i;
					}

				}
		
			},
			"./shaders",
			this
		);

		#endif

		*/

		//Create primitive buffer

		List<BufferAttributes> attrib{ { 0, GPUFormat::RGB32f }, { 1, GPUFormat::RG32f } };

		const List<Vec3f32> positions{

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

		const List<u16> iboBuffer{
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
					BufferLayout(positions, attrib[0]),
					BufferLayout(uvBuffer, attrib[1])
				},
				BufferLayout(iboBuffer, { 0, GPUFormat::R16u })
			)
		};

		//Create uniform buffer

		raygenData = {
			g, NAME("Raygen uniform buffer"),
			ShaderBuffer::Info(
				GPUBufferType::UNIFORM, GPUMemoryUsage::CPU_ACCESS,
				{ { NAME("mask"), ShaderBuffer::Layout(0, Buffer(sizeof(GPUData))) } }
			)
		};

		gpuData = (GPUData*) raygenData->getBuffer();

		gpuData->eye = eye;
		gpuData->triCount = triCount;
		gpuData->sphereCount = sphereCount;
		gpuData->cubeCount = cubeCount;
		gpuData->planeCount = planeCount;
		gpuData->skyboxColor = { 0, 0.5f, 1 };
		gpuData->exposure = 1;
		gpuData->displayType = DisplayType::Default;

		//Sphere y are inversed?

		sphereData = {
			g, NAME("Sphere data"),
			ShaderBuffer::Info(
				GPUBufferType::STRUCTURED, GPUMemoryUsage::CPU_ACCESS,
				{ { NAME("spheres"), ShaderBuffer::Layout(0, sphereCount * sizeof(Vec4f32)) } }
			)
		};

		Vec4f32 planes[] = {
			Vec4f32(0, 1, 0, -3)
		};

		planeData = {
			g, NAME("Plane data"),
			ShaderBuffer::Info(
				GPUBufferType::STRUCTURED, GPUMemoryUsage::LOCAL,
				{ { NAME("planes"), ShaderBuffer::Layout(0, Buffer((u8*)planes, (u8*)planes + sizeof(planes))) } }
			)
		};

		Cube cubes[] = {
			{ { -3, -3, -3 }, 0, { -2, -2, -2 }, 6 }
		};

		cubeData = {
			g, NAME("Cube data"),
			ShaderBuffer::Info(
				GPUBufferType::STRUCTURED, GPUMemoryUsage::LOCAL,
				{ { NAME("cubes"), ShaderBuffer::Layout(0, Buffer((u8*)cubes, (u8*)cubes + sizeof(cubes))) } }
			)
		};

		Triangle triangles[] = {
			{ Vec3f32(1, 1, 0), 0, Vec3f32(-1, 1, 0), 0, Vec3f32(1, -1, 1), 0 },
			{ Vec3f32(-1, 4, 0), 0, Vec3f32(1, 4, 0), 1, Vec3f32(1, 3, 1), 0 },
			{ Vec3f32(-1, 7, 0), 0, Vec3f32(1, 7, 0), 2, Vec3f32(1, 5, 1), 0 }
		};

		triangleData = {
			g, NAME("Triangle data"),
			ShaderBuffer::Info(
				GPUBufferType::STRUCTURED, GPUMemoryUsage::LOCAL,
				{ { NAME("triangles"), ShaderBuffer::Layout(0, Buffer((u8*)triangles, (u8*)triangles + sizeof(triangles))) } }
			)
		};

		Material materials[] = {
			{ { 1, 0.5, 1 }, 0,	{ 0.05f, 0.01f, 0.05f }, 1,		{ 0, 0, 0 }, 0, {}, 0 },
			{ { 0, 1, 0 },	 0,	{ 0, 0.05f, 0 },		 1,		{ 0, 0, 0 }, 0, {}, 0 },
			{ { 0, 0, 1 },	 0,	{ 0, 0, 0.05f },		 1,		{ 0, 0, 0 }, 0, {}, 0 },
			{ { 1, 0, 1 },	 0,	{ 0.05f, 0, 0.05f },	 1,		{ 0, 0, 0 }, 0, {}, 0 },
			{ { 1, 1, 0 },	 0,	{ 0.05f, 0.05f, 0 },	 1,		{ 0, 0, 0 }, 0, {}, 0 },
			{ { 0, 1, 1 },	 0,	{ 0, 0.05f, 0.05f },	 1,		{ 0, 0, 0 }, 0, {}, 0 },
			{ { 0, 0, 0 },	 1,	{ 0, 0, 0 },			 0,		{ 0, 0, 0 }, 0, {}, 0 },
			{ { 0, 0, 0 },	 .25,{ 0, 0, 0 },			 .5,	{ 0, 0, 0 }, 0, {}, 0 }
		};

		materialData = {
			g, NAME("Material data"),
			ShaderBuffer::Info(
				GPUBufferType::STRUCTURED, GPUMemoryUsage::LOCAL,
				{ { NAME("materials"), ShaderBuffer::Layout(0, Buffer((u8*)materials, (u8*)materials + sizeof(materials))) } }
			)
		};

		Light lights[] = {
			{ { 0, 0, 0 }, 0, { 0, 1, 0 }, 0, { 0.4f, 0.2f, 0.4f }, 0 },
			{ { 0, 0, 0 }, 5, { 0, 0, 0 }, 1, { 1.5f, 1.5f, 1.5f }, 0 },
			{ { -2, -2, -2 }, 7, { 0, 0, 0 }, 1, { 0.7f, 0.5f, 1.5f }, 0 }
		};

		lightData = {
			g, NAME("Lights"),
			ShaderBuffer::Info(
				GPUBufferType::STRUCTURED, GPUMemoryUsage::LOCAL,
				{ { NAME("lights"), ShaderBuffer::Layout(0, Buffer((u8*)lights, (u8*)lights + sizeof(lights))) } }
			)
		};

		counterBuffer = {
			g, NAME("Counter buffer"),
			ShaderBuffer::Info(
				GPUBufferType::STORAGE, GPUMemoryUsage::GPU_WRITE,
				{ { NAME("counterBuffer"), ShaderBuffer::Layout(0, sizeof(CounterBuffer)) } }
			)
		};

		dispatchArgs = {
			g, NAME("Dispatch args"),
			ShaderBuffer::Info(
				GPUBufferType::STORAGE, GPUMemoryUsage::GPU_WRITE,
				{ { NAME("dispatchArgs"), ShaderBuffer::Layout(0, sizeof(Vec4u32) * 2) } }
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

		//Reserve command list
		cl = { g, NAME("Command list"), CommandList::Info(1_KiB) };

		//Create descriptors

		auto descriptorsInfo = Descriptors::Info(pipelineLayout, {});
		descriptorsInfo.resources[0] = nullptr;
		descriptorsInfo.resources[1] = { raygenData, 0 };
		descriptorsInfo.resources[2] = { sphereData, 0 };
		descriptorsInfo.resources[3] = { materialData, 0 };
		descriptorsInfo.resources[4] = { planeData, 0 };
		descriptorsInfo.resources[5] = { triangleData, 0 };
		descriptorsInfo.resources[6] = nullptr;
		descriptorsInfo.resources[7] = { counterBuffer, 0 };
		descriptorsInfo.resources[8] = { dispatchArgs, 0 };
		descriptorsInfo.resources[9] = { lightData, 0 };
		descriptorsInfo.resources[12] = { cubeData, 0 };

		raytracingDescriptors = {
			g, NAME("Raytracing descriptors"),
			descriptorsInfo
		};

		//Prepare shaders

		makePipelines(0xFF, true);
	}

	~RaytracingInterface() {
		oic::System::files()->removeFileChangeCallback("./shaders");
	}

	//Create viewport resources

	void init(ViewportInfo* vp) final override {

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

	void release(const ViewportInfo*) final override {
		swapchain.release();
	}

	//Helper functions

	void updateUniforms() {

		const f32 aspect = res.cast<Vec2f32>().aspect();
		const f32 nearPlane = f32(std::tan(fov * 0.5_deg));

		v = Mat4x4f32::lookDirection(-gpuData->eye, eyeDir, { 0, 1, 0 });

		const Vec4f32 p0 = v * Vec4f32(-aspect, 1, -nearPlane, 1);
		const Vec4f32 p1 = v * Vec4f32(aspect, 1, -nearPlane, 1);
		const Vec4f32 p2 = v * Vec4f32(-aspect, -1, -nearPlane, 1);

		gpuData->p0 = p0.cast<Vec3f32>();
		gpuData->p1 = p1.cast<Vec3f32>();
		gpuData->p2 = p2.cast<Vec3f32>();

		gpuData->randomX = r.range<f32>(-100, 100);
		gpuData->randomY = r.range<f32>(-100, 100);

		++gpuData->sampleCount;

		memcpy(raygenData->getBuffer(), gpuData, sizeof(*gpuData));
		raygenData->flush(0, sizeof(*gpuData));
	}

	//Update size of surfaces

	void resize(const ViewportInfo*, const Vec2u32& size) final override {

		res = size;
		sampleCount = 0;

		gpuData->width = size.x;
		gpuData->height = size.y;
		gpuData->invRes = Vec2f32(1.f / size.x, 1.f / size.y);

		gui.resize(size);
		
		//Update compute target

		raytracingOutput.release();
		raytracingOutput = {
			g, NAME("Raytracing output"),
			Texture::Info(
				size.cast<Vec2u16>(), 
				GPUFormat::RGBA16f, GPUMemoryUsage::GPU_WRITE_ONLY, 
				1, 1
			)
		};

		raytracingAccumulation.release();
		raytracingAccumulation = {
			g, NAME("Raytracing accumulation"),
			Texture::Info(
				size.cast<Vec2u16>(), 
				GPUFormat::RGBA32f, GPUMemoryUsage::GPU_WRITE_ONLY, 
				1, 1
			)
		};

		reflectionBuffer.release();
		reflectionBuffer = {
			g, NAME("Reflection buffer"),
			Texture::Info(
				size.cast<Vec2u16>(), 
				GPUFormat::RGBA16f, GPUMemoryUsage::GPU_WRITE_ONLY, 
				1, 1
			)
		};

		shadowRayData.release();
		shadowRayData = {
			g, NAME("Shadow ray buffer"),
			ShaderBuffer::Info(
				GPUBufferType::STRUCTURED, GPUMemoryUsage::GPU_WRITE_ONLY,
				{ { NAME("shadowRays"), ShaderBuffer::Layout(0, sizeof(ShadowPayload) * maxShadowRaysPerPixel * size.prod()) } }
			)
		};

		shadowColors.release();
		shadowColors = {
			g, NAME("Shadow color buffer"),
			ShaderBuffer::Info(
				GPUBufferType::STRUCTURED, GPUMemoryUsage::GPU_WRITE_ONLY,
				{ { NAME("shadowColors"), ShaderBuffer::Layout(0, sizeof(Vec2u32) * maxShadowRaysPerPixel * size.prod()) } }
			)
		};

		shadowOutput.release();
		shadowOutput = {
			g, NAME("Shadow output buffer"),
			ShaderBuffer::Info(
				GPUBufferType::STRUCTURED, GPUMemoryUsage::GPU_WRITE_ONLY,
				{ { NAME("shadowOutput"), ShaderBuffer::Layout(0, u64(std::ceil(maxShadowRaysPerPixel * size.prod() / 32.0)) * 4) } }
			)
		};

		reflectionRayData.release();
		reflectionRayData = {
			g, NAME("Reflection ray buffer"),
			ShaderBuffer::Info(
				GPUBufferType::STRUCTURED, GPUMemoryUsage::GPU_WRITE_ONLY,
				{ { NAME("reflectionRays"), ShaderBuffer::Layout(0, sizeof(RayPayload) * size.prod()) } }
			)
		};

		positionBuffer.release();
		positionBuffer = {
			g, NAME("Position buffer"),
			ShaderBuffer::Info(
				GPUBufferType::STRUCTURED, GPUMemoryUsage::GPU_WRITE_ONLY,
				{ { NAME("positionBuffer"), ShaderBuffer::Layout(0, sizeof(Vec4f32) * size.prod()) } }
			)
		};

		gbuffer.release();
		gbuffer = {
			g, NAME("HitBuffer"),
			ShaderBuffer::Info(
				GPUBufferType::STRUCTURED, GPUMemoryUsage::GPU_WRITE_ONLY,
				{ { NAME("hitBuffer"), ShaderBuffer::Layout(0, sizeof(Hit) * size.prod()) } }
			)
		};

		raytracingDescriptors->updateDescriptor(0, { raytracingOutput, TextureType::TEXTURE_2D });
		raytracingDescriptors->updateDescriptor(6, { shadowRayData, 0 });
		raytracingDescriptors->updateDescriptor(10, { positionBuffer, 0 });
		raytracingDescriptors->updateDescriptor(11, { reflectionRayData, 0 });
		raytracingDescriptors->updateDescriptor(13, { raytracingAccumulation, TextureType::TEXTURE_2D });
		raytracingDescriptors->updateDescriptor(14, { reflectionBuffer, TextureType::TEXTURE_2D });
		raytracingDescriptors->updateDescriptor(15, { shadowOutput, 0 });
		raytracingDescriptors->updateDescriptor(16, { shadowColors, 0 });
		raytracingDescriptors->updateDescriptor(17, { gbuffer, 0 });
		raytracingDescriptors->updateDescriptor(18, { samplerNearest, gui.getFramebuffer()->getTarget(0), TextureType::TEXTURE_MS });
		raytracingDescriptors->flush({ { 0, 1 }, { 6, 1 }, { 10, 2 }, { 13, 6 } });

		swapchain->onResize(size);

		fillCommandList();
	}

	void fillCommandList() {

		cl->clear();

		cl->add(

			//Prepare data

			FlushBuffer(raygenData, uploadBuffer),
			FlushBuffer(lightData, uploadBuffer),
			FlushBuffer(materialData, uploadBuffer),
			FlushBuffer(triangleData, uploadBuffer),
			FlushBuffer(cubeData, uploadBuffer),
			FlushBuffer(planeData, uploadBuffer),
			FlushBuffer(sphereData, uploadBuffer),
			FlushBuffer(mesh, uploadBuffer),
			FlushImage(tex2D, uploadBuffer),

			ClearBuffer(counterBuffer),

			//Dispatch rays

			BindPipeline(raygenPipeline),
			BindDescriptors(raytracingDescriptors),
			Dispatch(res.cast<Vec2u32>()),

			//Prepare dispatch

			BindPipeline(dispatchSetup),
			Dispatch(1),

			//Shadows

			BindPipeline(shadowPipeline),
			DispatchIndirect(dispatchArgs, 0),

			//Reflections

			BindPipeline(reflectionPipeline),
			Dispatch(res.cast<Vec2u32>()),

			//Do gamma and color correction

			BindPipeline(postProcessing),
			Dispatch(res.cast<Vec2u32>())
		);
	}

	void makePipelines(u8 modified, bool fromExe) {

		for (auto &pipeline : pipelines)
			pipeline[0] = fromExe ? '~' : '.';

		if (modified & 1) {

			Buffer raygenComp;
			oicAssert("Couldn't load raygen shader", oic::System::files()->read(pipelines[0], raygenComp));

			raygenPipeline.release();
			raygenPipeline = {
				g, NAME("Raygen pipeline"),
				Pipeline::Info(
					Pipeline::Flag::NONE,
					raygenComp,
					pipelineLayout,
					Vec3u32{ THREADS_X, THREADS_Y, 1 }
				)
			};
		}

		if (modified & 2) {

			Buffer dispatch;
			oicAssert("Couldn't load dispatch shader", oic::System::files()->read(pipelines[1], dispatch));

			dispatchSetup.release();
			dispatchSetup = {
				g, NAME("Dispatch pipeline"),
				Pipeline::Info(
					Pipeline::Flag::NONE,
					dispatch,
					pipelineLayout,
					Vec3u32{ 1, 1, 1 }
				)
			};
		}

		if (modified & 4) {

			Buffer shadow;
			oicAssert("Couldn't load shadow shader", oic::System::files()->read(pipelines[2], shadow));

			shadowPipeline.release();
			shadowPipeline = {
				g, NAME("Shadow pipeline"),
				Pipeline::Info(
					Pipeline::Flag::NONE,
					shadow,
					pipelineLayout,
					Vec3u32{ THREADS, 1, 1 }
				)
			};
		}

		if (modified & 8) {

			Buffer postProcess;
			oicAssert("Couldn't load post_processing shader", oic::System::files()->read(pipelines[3], postProcess));

			postProcessing.release();
			postProcessing = {
				g, NAME("Post processing pipeline"),
				Pipeline::Info(
					Pipeline::Flag::NONE,
					postProcess,
					pipelineLayout,
					Vec3u32{ 8, 8, 1 }
				)
			};
		}

		if (modified & 16) {

			Buffer reflectionShader;
			oicAssert("Couldn't load reflection shader", oic::System::files()->read(pipelines[4], reflectionShader));

			reflectionPipeline.release();
			reflectionPipeline = {
				g, NAME("Reflection pipeline"),
				Pipeline::Info(
					Pipeline::Flag::NONE,
					reflectionShader,
					pipelineLayout,
					Vec3u32{ THREADS_X, THREADS_Y, 1 }
				)
			};
		}

		if (!cl->empty())
			fillCommandList();
	}

	//Execute commandList

	void render(const ViewportInfo *vi) final override {
		gui.render(g, vi->offset, vi->monitors);
		g.present(raytracingOutput, 0, 0, swapchain, gui.getCommands(), cl);
	}

	f64 time = 0;

	bool isShift{}, isCtrl{};
	Vec3f32 inputDir{};
	f32 fovChangeDir{};

	//Update eye
	void update(const ViewportInfo* vi, f64 dt) final override {

		Vec4f32 spheres[sphereCount] = {
			Vec4f32(0, 0, 5, 1),
			Vec4f32(0, 0, -5, 1),
			Vec4f32(3, 0, 0, 1),
			Vec4f32(0, -5, 0, 1),
			Vec4f32(7, f32(sin(time)), 0, 1),
			Vec4f32(-5 + f32(sin(time)), f32(cos(time)), 0, 1),
			Vec4f32(f32(sin(time)), 2 + f32(cos(time)), 0, 1)
		};

		std::memcpy(sphereData->getBuffer(), spheres, sizeof(spheres));
		sphereData->flush(0, sizeof(spheres));

		//TODO: Check artifacts at top of the screen

		#ifndef NDEBUG

		if (pipelineUpdates) {
			makePipelines(pipelineUpdates, false);
			pipelineUpdates = 0;
		}

		#endif

		for (auto* dvc : vi->devices)
			if (dvc->isType(InputDevice::MOUSE)) {

				f64 delta = dvc->getCurrentAxis(MouseAxis::Axis_wheel_y);

				//TODO: Mouse wheel seems to hang?

				if (oic::Math::abs(delta) > 0.02)
					speed = oic::Math::clamp(speed * 1 + (delta / 1024), 0.5, 5.0);
			}

		if(isCtrl)
			time += dt * 0.75;
		else
			time += dt * 0.05;

		fov = f32(oic::Math::clamp(fov - fovChangeSpeed * fovChangeDir * dt, 0.99999999, 149.99999999));

		if (inputDir.any()) {

			const f32 speedUp = isShift ? 10.f : 1.f;

			Vec3f32 d = inputDir.clamp(-1, 1) * f32(dt * speed * speedUp);

			if(d.any())
				sampleCount = 0;

			gpuData->eye -= (v * Vec4f32(d.x, d.y, d.z, 0)).cast<Vec3f32>();
		}

		if (res.neq(0).all())
			updateUniforms();
	}

	//Input

	void onInputUpdate(ViewportInfo*, const InputDevice *dvc, InputHandle ih, bool isActive) final override {

		isActive &= !gui.onInputUpdate(dvc, ih, isActive);
			
		if (!dynamic_cast<const Keyboard*>(dvc))
			return;

		if (ih == Key::Key_w) inputDir.z = isActive;
		if (ih == Key::Key_s) inputDir.z = -f32(isActive);

		if (ih == Key::Key_q) inputDir.y = -f32(isActive);
		if (ih == Key::Key_e) inputDir.y = isActive;

		if (ih == Key::Key_d) inputDir.x = -f32(isActive);
		if (ih == Key::Key_a) inputDir.x = isActive;

		if (ih == Key::Key_1) fovChangeDir = -f32(isActive);
		if (ih == Key::Key_2) fovChangeDir = isActive;

		if (ih == Key::Key_shift)
			isShift = isActive;

		if (ih == Key::Key_ctrl)
			isCtrl = isActive;
	}
};

//Create window and wait for exit

int main() {

	Graphics g("Igx raytracing test", 1, "Igx", 1);
	RaytracingInterface viewportInterface(g);

	g.pause();

	System::viewportManager()->create(
		ViewportInfo(
			g.appName, {}, {}, 0, &viewportInterface, ViewportInfo::HANDLE_INPUT
		)
	);

	System::viewportManager()->waitForExit();

	return 0;
}