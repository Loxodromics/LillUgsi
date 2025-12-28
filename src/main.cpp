#include "core/application.h"
#ifdef USE_PLANET
#include "planet/planetdata.h"
#endif

#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>
#include <iostream>

/// RAII wrapper for spdlog lifetime management
/// Ensures spdlog is properly initialized at startup and shutdown at exit
/// This prevents crashes when logging during cleanup after static destructors run
struct SpdlogLifetime {
	SpdlogLifetime() {
		spdlog::set_level(spdlog::level::debug);
	}

	~SpdlogLifetime() {
		spdlog::shutdown();
	}
};

int main(int argc, char* argv[]) {
	/// Initialize spdlog with RAII lifetime management
	/// This ensures spdlog::shutdown() is called on all exit paths
	SpdlogLifetime spdlogLifetime;

	/// Parse command-line arguments
	std::optional<float> timeoutSeconds;
	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i];
		if (arg == "--timeout" || arg == "-t") {
			if (i + 1 < argc) {
				try {
					float timeout = std::stof(argv[i + 1]);
					if (timeout <= 0.0f) {
						spdlog::error("Timeout must be positive, got: {}", timeout);
						return 1;
					}
					timeoutSeconds = timeout;
					i++;  /// Skip next argument (the timeout value)
				} catch (const std::exception& e) {
					spdlog::error("Invalid timeout value '{}': {}", argv[i + 1], e.what());
					return 1;
				}
			} else {
				spdlog::error("--timeout requires a value in seconds");
				return 1;
			}
		} else if (arg == "--help" || arg == "-h") {
			std::cout << "LillUgsi Vulkan Learning Renderer\n";
			std::cout << "Usage: " << argv[0] << " [options]\n";
			std::cout << "Options:\n";
			std::cout << "  --timeout, -t <seconds>  Exit after specified seconds\n";
			std::cout << "  --help, -h               Show this help message\n";
			return 0;
		} else {
			spdlog::warn("Unknown argument: {}", arg);
		}
	}

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

		spdlog::info("Starting LillUgsi Vulkan Learning Renderer");

		/// Create and initialize the application
		lillugsi::core::Application app("LillUgsi: Vulkan Learning Renderer", 800, 600);

		/// Apply timeout if specified
		if (timeoutSeconds.has_value()) {
			app.setTimeoutSeconds(timeoutSeconds.value());
		}

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