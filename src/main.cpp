#include "core/application.h"
#include <spdlog/spdlog.h>
#include <stdexcept>

int main(int argc, char* argv[]) {
	try {
		/// Initialize spdlog
		spdlog::set_level(spdlog::level::debug);
		spdlog::info("Starting Vulkan Learning Renderer");

		/// Create and initialize the application
		lillugsi::core::Application app("Vulkan Learning Renderer", 800, 600);

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