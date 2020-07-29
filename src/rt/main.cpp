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
#include "igxi/convert.hpp"

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
	Intersection_side,
	Shadows_no_reflection,
	Dispatch_groups,
	Reflection_albedo,
	Reflection_normals,
	Reflection_material,
	Reflection_object,
	Reflection_intersection_side,
	Reflection_intersection_attributes,
	Reflection_of_reflection
);

oicExposedEnum(
	ProjectionType, u32, 
	Default, 
	Omnidirectional,
	Equirect,
	Stereoscopic_omnidirectional_TB,
	Stereoscopic_TB,
	Stereoscopic_omnidirectional_LR,
	Stereoscopic_LR
);

struct RaytracingInterface : public ViewportInterface {

	Graphics& g;

	//Resources

	GUI gui;

	SwapchainRef swapchain;
	CommandListRef cl;
	PrimitiveBufferRef mesh;
	DescriptorsRef raytracingDescriptors;
	PipelineRef raygenPipeline, initPipeline, shadowPipeline, postProcessing, reflectionPipeline;
	TextureRef raytracingOutput, raytracingAccumulation, reflectionBuffer, skybox;
	SamplerRef samp;

	GPUBufferRef 
		rayConstData, sphereData, planeData, triangleData, lightData,
		materialData, cubeData, shadowOutput, gbuffer, initData;

	UploadBufferRef uploadBuffer;

	Vec2u32 res;
	Vec3f32 eye{ 4, 2, -2 }, eyeDir = { 0, 0, -1 };
	Mat4x4f32 v = Mat4x4f32::lookDirection(eye, eyeDir, { 0, 1, 0 });

	SamplerRef samplerNearest, samplerLinear;

	f64 speed = 5, fovChangeSpeed = 7;
	f32 fov = 70;

	static constexpr usz sphereCount = 7, triCount = 3, cubeCount = 2, planeCount = 1, lightCount = 3;

	oic::Random r;

	u8 pipelineUpdates{};

	String pipelines[5] = {
		"./shaders/raygen.comp.spv",
		"./shaders/init.comp.spv",
		"./shaders/shadow.comp.spv",
		"./shaders/post_processing.comp.spv",
		"./shaders/reflection.comp.spv"
	};

	struct InitData {
		f32 randomX, randomY;
		f32 cpuOffsetX, cpuOffsetY;
		u32 sampleCount;
	};

	//Data required for raytracing
	struct GPUData {

		Vec3f32 eye;
		u32 width;
			
		Vec3f32 p0;
		u32 height;
		
		Vec3f32 p1;
		f32 ipd;
		
		Vec3f32 p2;
		ProjectionType projectionType;

		Vec3f32 skyboxColor; f32 exposure;

		Vec2f32 invRes;
		f32 focalDistance;
		f32 aperature;

		bool disableUI; u8 padding0[3];
		u32 triCount;
		u32 sphereCount;
		u32 cubeCount;

		u32 planeCount;
		DisplayType displayType;
		bool enableSkybox; u8 padding1[3];

		InflectWithName(
			{
				"Eye",
				"Skybox color",
				"Exposure",
				"Display time",
				"Projection type",
				"Interpupillary distance (mm)",
				"Enable skybox"
			},
			eye,
			skyboxColor,
			exposure,
			displayType,
			projectionType,
			ipd,
			enableSkybox
		);

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
	InitData *initDataBuf{};

	Vec2u16 targetSize = { 7680, 4320 };
	u16 targetSamples = 64;

	bool shouldOutputNextFrame{};
	bool isResizeRequested{};

	String targetOutput = "./output/0";

	void outputNextFrame() const {
		(bool&) shouldOutputNextFrame = true;
	}

	void resetSamples() const {
		(u32&) initDataBuf->sampleCount = 0;
	}

	InflectWithName(
		{ 
			"", 
			"FOV", "FOV Change speed", 
			"Eye dir", "Move speed", 
			"View matrix",
			"Target size", "Target samples",
			"Target output path (relative to ./)",
			"Output next frame",
			"Reset accumulation samples"
		}, 
		*gpuData, 
		fov, fovChangeSpeed, 
		eyeDir, speed, 
		(const Mat4x4f32&) v,
		targetSize, targetSamples,
		targetOutput,
		Button<RaytracingInterface, &RaytracingInterface::outputNextFrame>{},
		Button<RaytracingInterface, &RaytracingInterface::resetSamples>{}
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

		samplerLinear = {
			g, NAME("Linear sampler"),
			Sampler::Info(
				SamplerMin::LINEAR, SamplerMag::LINEAR,
				SamplerMode::CLAMP_BORDER, 1
			)
		};

		pipelineLayout = {

			g, NAME("Raytracing pipeline layout"),

			PipelineLayout::Info(
				RegisterLayout(NAME("Output"), 0, TextureType::TEXTURE_2D, 0, ShaderAccess::COMPUTE, GPUFormat::RGBA8, true),
				RegisterLayout(NAME("Raygen data"), 1, GPUBufferType::UNIFORM, 0, ShaderAccess::COMPUTE, sizeof(GPUData)),
				RegisterLayout(NAME("Spheres"), 2, GPUBufferType::STRUCTURED, 0, ShaderAccess::COMPUTE, sizeof(Vec4f32)),
				RegisterLayout(NAME("Planes"), 3, GPUBufferType::STRUCTURED, 1, ShaderAccess::COMPUTE, sizeof(Vec4f32)),
				RegisterLayout(NAME("Triangles"), 4, GPUBufferType::STRUCTURED, 2, ShaderAccess::COMPUTE, sizeof(Triangle)),
				RegisterLayout(NAME("Materials"), 5, GPUBufferType::STRUCTURED, 3, ShaderAccess::COMPUTE, sizeof(Material)),
				RegisterLayout(NAME("Lights"), 6, GPUBufferType::STRUCTURED, 7, ShaderAccess::COMPUTE, sizeof(Light)),
				RegisterLayout(NAME("Cubes"), 7, GPUBufferType::STRUCTURED, 10, ShaderAccess::COMPUTE, sizeof(Cube)),
				RegisterLayout(NAME("Accumulation buffer"), 8, TextureType::TEXTURE_2D, 1, ShaderAccess::COMPUTE, GPUFormat::RGBA32f, true),
				RegisterLayout(NAME("Reflection buffer"), 9, TextureType::TEXTURE_2D, 2, ShaderAccess::COMPUTE, GPUFormat::RGBA16f, true),
				RegisterLayout(NAME("Shadow output"), 10, GPUBufferType::STRUCTURED, 11, ShaderAccess::COMPUTE, sizeof(u32)),
				RegisterLayout(NAME("HitBuffer"), 11, GPUBufferType::STRUCTURED, 13, ShaderAccess::COMPUTE, sizeof(Hit)),
				RegisterLayout(NAME("UI"), 12, SamplerType::SAMPLER_MS, 0, ShaderAccess::COMPUTE),
				RegisterLayout(NAME("Init"), 13, GPUBufferType::STORAGE, 14, ShaderAccess::COMPUTE, sizeof(InitData)),
				RegisterLayout(NAME("Skybox"), 14, SamplerType::SAMPLER_2D, 1, ShaderAccess::COMPUTE)
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

		rayConstData = {
			g, NAME("Raygen uniform buffer"),
			GPUBuffer::Info(
				sizeof(GPUData), GPUBufferType::UNIFORM, GPUMemoryUsage::CPU_WRITE
			)
		};

		gpuData = (GPUData*) rayConstData->getBuffer();

		gpuData->eye = eye;
		gpuData->triCount = triCount;
		gpuData->sphereCount = sphereCount;
		gpuData->cubeCount = cubeCount;
		gpuData->planeCount = planeCount;
		gpuData->skyboxColor = { 0, 0.5f, 1 };
		gpuData->exposure = 1;
		gpuData->ipd = 6.2f;
		gpuData->enableSkybox = true;

		//Sphere y are inversed?

		sphereData = {
			g, NAME("Sphere data"),
			GPUBuffer::Info(
				sphereCount * sizeof(Vec4f32), GPUBufferType::STRUCTURED, GPUMemoryUsage::CPU_WRITE
			)
		};

		Vec4f32 planes[planeCount] = {
			Vec4f32(0, -1, 0, 0)
		};

		planeData = {
			g, NAME("Plane data"),
			GPUBuffer::Info(
				Buffer((const u8*)planes, (const u8*)planes + sizeof(planes)), GPUBufferType::STRUCTURED, GPUMemoryUsage::LOCAL
			)
		};

		//TODO: If kS == 0, there won't be a reflection
		//		If kD == 0, there won't be shadow rays

		Cube cubes[cubeCount] = {
			{ { 0, 0, 0 }, 0, { 1, 1, 1 }, 1 },
			{ { -2, 0, -2 }, 1, { -1, 1, -1 }, 6 }
		};

		cubeData = {
			g, NAME("Cube data"),
			GPUBuffer::Info(
				Buffer((const u8*)cubes, (const u8*)cubes + sizeof(cubes)),
				GPUBufferType::STRUCTURED, GPUMemoryUsage::LOCAL
			)
		};

		Triangle triangles[triCount] = {
			{ Vec3f32(1, 1, 0), 0, Vec3f32(-1, 1, 0), 0, Vec3f32(1, 0, 1), 0 },
			{ Vec3f32(-1, 4, 0), 0, Vec3f32(1, 4, 0), 1, Vec3f32(1, 3, 1), 0 },
			{ Vec3f32(-1, 7, 0), 0, Vec3f32(1, 7, 0), 2, Vec3f32(1, 5, 1), 0 }
		};

		triangleData = {
			g, NAME("Triangle data"),
			GPUBuffer::Info(
				Buffer((const u8*)triangles, (const u8*)triangles + sizeof(triangles)),
				GPUBufferType::STRUCTURED, GPUMemoryUsage::LOCAL
			)
		};

		initData = {
			g, NAME("Init data"),
			GPUBuffer::Info(
				sizeof(InitData),
				GPUBufferType::STORAGE, GPUMemoryUsage::GPU_WRITE | GPUMemoryUsage::CPU_WRITE
			)
		};

		initDataBuf = (InitData*) initData->getBuffer();

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
			GPUBuffer::Info(
				Buffer((const u8*)materials, (const u8*)materials + sizeof(materials)),
				GPUBufferType::STRUCTURED, GPUMemoryUsage::LOCAL
			)
		};

		Light lights[lightCount] = {
			{ { 0, 0, 0 }, 0, { 0, -1, 0 }, 0, { 0.4f, 0.2f, 0.4f }, 0 },
			{ { 0, 0.1f, 0 }, 5, { 0, 0, 0 }, 1, { 1.5f, 1.5f, 1.5f }, 0 },
			{ { 2, 2, 2 }, 7, { 0, 0, 0 }, 1, { 0.7f, 0.5f, 1.5f }, 0 }
		};

		lightData = {
			g, NAME("Lights"),
			GPUBuffer::Info(
				Buffer((const u8*)lights, (const u8*)lights + sizeof(lights)),
				GPUBufferType::STRUCTURED, GPUMemoryUsage::LOCAL
			)
		};

		//Create texture

		skybox = {
			g, NAME("Skybox"),
			igxi::Helper::loadDiskExternal("./textures/cape_hill_4k.hdr", g)
		};

		//Create sampler

		samp = { g, NAME("Test sampler"), Sampler::Info() };

		//Reserve command list
		cl = { g, NAME("Command list"), CommandList::Info(1_KiB) };

		//Create descriptors

		auto descriptorsInfo = Descriptors::Info(pipelineLayout, {});
		descriptorsInfo.resources[1] = { rayConstData, 0 };
		descriptorsInfo.resources[2] = { sphereData, 0 };
		descriptorsInfo.resources[3] = { planeData, 0 };
		descriptorsInfo.resources[4] = { triangleData, 0 };
		descriptorsInfo.resources[5] = { materialData, 0 };
		descriptorsInfo.resources[6] = { lightData, 0 };
		descriptorsInfo.resources[7] = { cubeData, 0 };
		descriptorsInfo.resources[13] = { initData, 0 };
		descriptorsInfo.resources[14] = { samplerLinear, skybox, TextureType::TEXTURE_2D };

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

	void updateUniforms(bool stationary) {

		const f32 aspect = res.cast<Vec2f32>().aspect();
		const f32 nearPlane = f32(std::tan(fov * 0.5_deg));

		v = Mat4x4f32::lookDirection(-gpuData->eye, eyeDir, { 0, 1, 0 });

		const Vec4f32 p0 = v * Vec4f32(-aspect, 1, -nearPlane, 1);
		const Vec4f32 p1 = v * Vec4f32(aspect, 1, -nearPlane, 1);
		const Vec4f32 p2 = v * Vec4f32(-aspect, -1, -nearPlane, 1);

		gpuData->p0 = p0.cast<Vec3f32>();
		gpuData->p1 = p1.cast<Vec3f32>();
		gpuData->p2 = p2.cast<Vec3f32>();

		initDataBuf->cpuOffsetX = r.range<f32>(-100, 100);
		initDataBuf->cpuOffsetY = r.range<f32>(-100, 100);

		if (stationary) {
			++initDataBuf->sampleCount;
			stationary = false;
		}
		else initDataBuf->sampleCount = 0;

		rayConstData->flush(0, sizeof(*gpuData));
		initData->flush(0, sizeof(*initDataBuf));
	}

	//Update size of surfaces

	void resize(const ViewportInfo*, const Vec2u32& size) final override {

		g.wait();					//Ensure we don't have any work pending

		res = size;
		initDataBuf->sampleCount = 0;

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
				GPUFormat::RGBA8, GPUMemoryUsage::GPU_WRITE_ONLY, 
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

		constexpr Vec2f32 perTile = Vec2f32(THREADS_X, 64 / THREADS_X);

		Vec2u16 tiles = (size.cast<Vec2f32>() / perTile).ceil().cast<Vec2u16>();

		shadowOutput.release();
		shadowOutput = {
			g, NAME("Shadow output buffer"),
			GPUBuffer::Info(
				sizeof(u64) * tiles.x * tiles.y,		//a uint64 per tile
				GPUBufferType::STRUCTURED, GPUMemoryUsage::GPU_WRITE_ONLY
			)
		};

		gbuffer.release();
		gbuffer = {
			g, NAME("HitBuffer"),
			GPUBuffer::Info(
				sizeof(Hit) * size.prod<usz>(),
				GPUBufferType::STRUCTURED, GPUMemoryUsage::GPU_WRITE_ONLY
			)
		};

		raytracingDescriptors->updateDescriptor(0, { raytracingOutput, TextureType::TEXTURE_2D });
		raytracingDescriptors->updateDescriptor(8, { raytracingAccumulation, TextureType::TEXTURE_2D });
		raytracingDescriptors->updateDescriptor(9, { reflectionBuffer, TextureType::TEXTURE_2D });
		raytracingDescriptors->updateDescriptor(10, { shadowOutput, 0 });
		raytracingDescriptors->updateDescriptor(11, { gbuffer, 0 });
		raytracingDescriptors->updateDescriptor(12, { samplerNearest, gui.getFramebuffer()->getTarget(0), TextureType::TEXTURE_MS });
		raytracingDescriptors->flush({ { 0, 1 }, { 8, 5 } });

		if(!isResizeRequested)
			swapchain->onResize(size);

		fillCommandList();
	}

	void fillCommandList() {

		cl->clear();

		cl->add(

			//Prepare data

			FlushBuffer(rayConstData, uploadBuffer),
			FlushBuffer(lightData, uploadBuffer),
			FlushBuffer(materialData, uploadBuffer),
			FlushBuffer(triangleData, uploadBuffer),
			FlushBuffer(cubeData, uploadBuffer),
			FlushBuffer(planeData, uploadBuffer),
			FlushBuffer(sphereData, uploadBuffer),
			FlushBuffer(mesh, uploadBuffer),
			FlushBuffer(initData, uploadBuffer),
			FlushImage(skybox, uploadBuffer),

			//Prepare all shaders

			BindDescriptors(raytracingDescriptors),
			BindPipeline(initPipeline),
			Dispatch(1),

			//Dispatch rays

			BindPipeline(raygenPipeline),
			Dispatch(res.cast<Vec2u32>()),

			//Shadows

			BindPipeline(shadowPipeline),
			Dispatch(Vec3u32(res.x, res.y, lightCount)),

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

			Buffer raygenComp = oic::System::files()->readToBuffer(pipelines[0]);

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

			Buffer initShader = oic::System::files()->readToBuffer(pipelines[1]);

			initPipeline.release();
			initPipeline = {
				g, NAME("Init shaders pipeline"),
				Pipeline::Info(
					Pipeline::Flag::NONE,
					initShader,
					pipelineLayout,
					Vec3u32{ 1, 1, 1 }
				)
			};
		}

		if (modified & 4) {

			Buffer shadow = oic::System::files()->readToBuffer(pipelines[2]);

			shadowPipeline.release();
			shadowPipeline = {
				g, NAME("Shadow pipeline"),
				Pipeline::Info(
					Pipeline::Flag::NONE,
					shadow,
					pipelineLayout,
					Vec3u32{ THREADS_X, THREADS_Y, 1 }
				)
			};
		}

		if (modified & 8) {

			Buffer postProcess = oic::System::files()->readToBuffer(pipelines[3]);

			postProcessing.release();
			postProcessing = {
				g, NAME("Post processing pipeline"),
				Pipeline::Info(
					Pipeline::Flag::NONE,
					postProcess,
					pipelineLayout,
					Vec3u32{ THREADS_X, THREADS_Y, 1 }
				)
			};
		}

		if (modified & 16) {

			Buffer reflectionShader = oic::System::files()->readToBuffer(pipelines[4]);

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

	void onRenderFinish(UploadBuffer *result, const Pair<u64, u64> &allocation, TextureObject *image, const Vec3u16&, const Vec3u16 &dim, u16, u8, bool) {

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

	void render(const ViewportInfo *vi) final override {

		if (shouldOutputNextFrame) {

			//Setup render

			isResizeRequested = true;
			resize(vi, targetSize.cast<Vec2u32>());

			bool disableUI = gpuData->disableUI;
			gpuData->disableUI = true;

			UploadBufferRef cpuOutput {
				g, "Frame output",
				UploadBuffer::Info(
					raytracingOutput->size(), 0, 0
				)
			};

			List<CommandList*> commands(gpuData->displayType == DisplayType::Accumulation ? targetSamples : 1);

			for (auto &cmd : commands)
				cmd = cl;

			g.presentToCpu<RaytracingInterface, &RaytracingInterface::onRenderFinish>(
				commands, raytracingOutput, cpuOutput, this
			);

			//Reset to old state and wait for work to finish

			Vec2u16 actualSize = swapchain->getInfo().size;
			resize(vi, Vec2u32(actualSize.x, actualSize.y));

			gpuData->disableUI = disableUI;

			isResizeRequested = false;
			shouldOutputNextFrame = false;
		}

		//Regular render

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
			Vec4f32(0, 1, 5, 1),
			Vec4f32(0, 1, -5, 1),
			Vec4f32(3, 1, 0, 1),
			Vec4f32(0, 6, 0, 1),
			Vec4f32(7, 2 + f32(sin(time)), 0, 1),
			Vec4f32(-5 + f32(sin(time)), 2 + f32(cos(time)), 0, 1),
			Vec4f32(f32(sin(time)), 3 + f32(cos(time)), 0, 1)
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

		bool stationary = true;

		if (inputDir.any()) {

			const f32 speedUp = isShift ? 10.f : 1.f;

			Vec3f32 d = inputDir.clamp(-1, 1) * f32(dt * speed * speedUp);

			if (d.any())
				stationary = false;

			gpuData->eye -= (v * Vec4f32(d.x, d.y, d.z, 0)).cast<Vec3f32>();
		}

		if (res.neq(0).all())
			updateUniforms(stationary);
	}

	//Input

	void onInputUpdate(ViewportInfo*, const InputDevice *dvc, InputHandle ih, bool isActive) final override {

		isActive &= !gui.onInputUpdate(dvc, ih, isActive);
			
		if (!dynamic_cast<const Keyboard*>(dvc))
			return;

		if (ih == Key::Key_s) inputDir.z = -f32(isActive);
		if (ih == Key::Key_w) inputDir.z = isActive;

		if (ih == Key::Key_e) inputDir.y = -f32(isActive);
		if (ih == Key::Key_q) inputDir.y = isActive;

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