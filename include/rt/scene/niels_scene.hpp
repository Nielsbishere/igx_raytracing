#pragma once
#include "helpers/scene_graph.hpp"

namespace igx::rt {

	class NielsScene : public SceneGraph {

		f64 time{};
		u64 dynamicObjects[3];

	public:

		NielsScene(FactoryContainer &factory);

		void update(f64 dt) override;

	};

}