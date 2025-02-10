#include "core/application.h"
#include "planet/planetdata.h"
#include <spdlog/spdlog.h>
#include <stdexcept>

int main(int argc, char* argv[]) {
	try {
		// spdlog::set_level(spdlog::level::trace);
		// const std::shared_ptr<lillugsi::planet::PlanetData> icosphere = std::make_shared<lillugsi::planet::PlanetData>();
		// icosphere->subdivide(2);

		// lillugsi::planet::DataSettingVisitor dataVisitor;
		// icosphere->applyFaceVisitor(dataVisitor);
		//
		// // lillugsi::planet::NoiseTerrainVisitor noiseVisitor;
		// // icosphere.applyVertexVisitor(noiseVisitor);
		//
		// std::shared_ptr<lillugsi::rendering::IcosphereMesh> icosphereMesh;
		// icosphereMesh = std::make_shared<lillugsi::rendering::IcosphereMesh>(1.0f, 2);
		//
		// lillugsi::planet::PlanetGenerator planetGenerator(icosphere, icosphereMesh);
		// planetGenerator.generateTerrain();
		// return 0;


		/// Initialize spdlog
		spdlog::set_level(spdlog::level::debug);
		spdlog::info("Starting LillUgsi Vulkan Learning Renderer");

		/// Create and initialize the application
		lillugsi::core::Application app("LillUgsi: Vulkan Learning Renderer", 800, 600);

		if (!app.initialize()) {
			spdlog::error("Failed to initialize the application");
			return 1;
		}

		/// Run the application
		app.run();

		/// Cleanup is handled by the Application destructor
		spdlog::info("Application exiting normally");
		return 0;
	}
	catch (const std::exception& e) {
		spdlog::error("Caught exception: {}", e.what());
		return 1;
	}
	catch (...) {
		spdlog::error("Caught unknown exception");
		return 1;
	}
}