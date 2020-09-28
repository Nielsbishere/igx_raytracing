#include "rt/scene/niels_scene.hpp"

namespace igx::rt {

	NielsScene::NielsScene(ui::GUI &gui, FactoryContainer &factory):
		SceneGraph(gui, factory, NAME("Niels scene"), VIRTUAL_FILE("~/textures/qwantani_4k.hdr"))
	{
		Vec3f32 sunDir = Vec3f32{ -0.5f, -2, -1 }.normalize();

		add(

			Material{ { 1, 0.5, 1 },	{ 0.05f, 0.01f, 0.05f },	{ 0, 0, 0 },	0,		1,		1 },
			Material{ { 0, 1, 0 },		{ 0, 0.05f, 0 },			{ 0, 0, 0 },	0,		1,		1 },
			Material{ { 0, 0, 1 },		{ 0, 0, 0.05f },			{ 0, 0, 0 },	0,		1,		1 },
			Material{ { 1, 0, 1 },		{ 0.05f, 0, 0.05f },		{ 0, 0, 0 },	0,		1,		1 },
			Material{ { 1, 1, 0 },		{ 0.05f, 0.05f, 0 },		{ 0, 0, 0 },	0,		1,		1 },
			Material{ { 0, 1, 1 },		{ 0, 0.05f, 0.05f },		{ 0, 0, 0 },	0,		1,		1 },
			Material{ { 0, 0, 0 },		{ 0, 0, 0 },				{ 0, 0, 0 },	1,		0,		1 },
			Material{ { 0, 0, 0 },		{ 0, 0, 0 },				{ 0, 0, 0 },	.25f,	.5f,	1 },

			Plane{ Vec3f32(0, 1, 0),	0 },					0_u32,

			Cube{ Vec3f32(0, 0, 0),	Vec3f32(1, 1, 1) },			1_u32,
			Cube{ Vec3f32(-2, 0, -2),	Vec3f32(-1, 1, -1) },	2_u32,
			
			Triangle{
				Vec3f32(1, 1, 0),
				Vec3f32(-1, 1, 0),
				Vec3f32(1, 0, 1)
			},													3_u32,

			Triangle{
				Vec3f32(-1, 4, 0),
				Vec3f32(1, 4, 0),
				Vec3f32(1, 3, 1)
			},													4_u32,

			Triangle{
				Vec3f32(-1, 7, 0),
				Vec3f32(1, 7, 0),
				Vec3f32(1, 5, 1)
			},													5_u32,

			Sphere{ Vec3f32{ 0, 1, 5 },	1 },					0_u32,
			Sphere{ Vec3f32{ 0, 1, -5 },	1 },				1_u32,
			Sphere{ Vec3f32{ 3, 1, 0 },	1 },					2_u32,
			Sphere{ Vec3f32{ 0, 6, 0 },	1 },					3_u32,

			Light{ sunDir,				Vec3f32(0.9f) },
			Light{ Vec3f32(0, 0.1f),	Vec3f32(1.0f, 0.f, 0.f),	 5,		0.3f },
			Light{ Vec3f32(2),			Vec3f32(0.0f, 1.0f, 1.0f),	 7,		0.6f }
		);

		dynamicObjects[0] = addGeometry(Sphere {}, 4_u32);
		dynamicObjects[1] = addGeometry(Sphere {}, 0_u32);
		dynamicObjects[2] = addGeometry(Sphere {}, 7_u32);

		update(0);
	}

	void NielsScene::update(f64 dt) {

		SceneGraph::update(dynamicObjects[0], Sphere { Vec3f32(7, 2 + f32(sin(0))),						1 });
		SceneGraph::update(dynamicObjects[1], Sphere{ Vec3f32(-5 + f32(sin(time)), 2 + f32(cos(time))),	1 });
		SceneGraph::update(dynamicObjects[2], Sphere{ Vec3f32(f32(sin(time)), 3 + f32(cos(time))),		1 });

		time += dt;

		SceneGraph::update(dt);
	}

}