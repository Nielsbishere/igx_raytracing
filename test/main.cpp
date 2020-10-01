#include "rt/raytracing_interface.hpp"
#include "scene/niels_scene.hpp"

//Create window and wait for exit

int main() {

	using namespace oic;

	ignis::Graphics g("Igx raytracing test", 1, "Igx", 1);

	igx::FactoryContainer factory(g);
	igx::ui::GUI gui(g);

	igx::rt::NielsScene nielscene(gui, factory);

	igx::rt::RaytracingInterface viewportInterface(g, gui, factory, nielscene);

	g.pause();

	System::viewportManager()->create(
		ViewportInfo(
			g.appName, {}, {}, 0, &viewportInterface, ViewportInfo::HANDLE_INPUT
		)
	);

	System::viewportManager()->waitForExit();

	return 0;
}