#include "rt/scene/niels_scene.hpp"

namespace igx::rt {

	NielsScene::NielsScene(FactoryContainer &factory):
		SceneGraph(factory, NAME("Niels scene"), VIRTUAL_FILE("~/textures/qwantani_4k.hdr"))
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

			Plane{ Vec3f32(0, 1, 0),	0 },

			Cube{ Vec3f32(0, 0, 0),		Vec3f32(1, 1, 1) },
			Cube{ Vec3f32(-2, 0, -2),	Vec3f32(-1, 1, -1) },
			
			Triangle{ Vec3f32(1, 1, 0),		Vec3f32(-1, 1, 0),	Vec3f32(1, 0, 1) },
			Triangle{ Vec3f32(-1, 4, 0),	Vec3f32(1, 4, 0),	Vec3f32(1, 3, 1) },
			Triangle{ Vec3f32(-1, 7, 0),	Vec3f32(1, 7, 0),	Vec3f32(1, 5, 1) },

			Sphere{ Vec3f32{ 0, 1, 5 },		1 },
			Sphere{ Vec3f32{ 0, 1, -5 },	1 },
			Sphere{ Vec3f32{ 3, 1, 0 },		1 },
			Sphere{ Vec3f32{ 0, 6, 0 },		1 },

			Light{ sunDir,				Vec3f32(0.7f) },
			Light{ Vec3f32(0, 0.1f),	Vec3f32(1.5f),				 5,		0.3f },
			Light{ Vec3f32(2),			Vec3f32(0.7f, 0.5f, 1.5f),	 7,		0.6f }
		);

		dynamicObjects[0] = add(Sphere{ Vec3f32(7, 2 + f32(sin(0))),						1 });
		dynamicObjects[1] = add(Sphere{ Vec3f32(-5 + f32(sin(time)), 2 + f32(cos(time))),	1 });
		dynamicObjects[2] = add(Sphere{ Vec3f32(f32(sin(time)), 3 + f32(cos(time))),		1 });
	}

	void NielsScene::update(f64 dt) {

		SceneGraph::update(dynamicObjects[0], Sphere{ Vec3f32(7, 2 + f32(sin(0))),						1 });
		SceneGraph::update(dynamicObjects[1], Sphere{ Vec3f32(-5 + f32(sin(time)), 2 + f32(cos(time))),	1 });
		SceneGraph::update(dynamicObjects[2], Sphere{ Vec3f32(f32(sin(time)), 3 + f32(cos(time))),		1 });

		time += dt;

		SceneGraph::update(dt);
	}

}