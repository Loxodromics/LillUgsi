#include "application.h"
#include <spdlog/spdlog.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

Application::Application(const std::string& appName, uint32_t width, uint32_t height)
	: appName(appName), width(width), height(height), window(nullptr), isRunning(false) {
}

Application::~Application() {
	this->cleanup();
}

bool Application::initialize() {
	/// Initialize SDL
	if (SDL_Init(SDL_INIT_VIDEO) != true) {
		spdlog::error("SDL could not initialize! SDL_Error: {}", SDL_GetError());
		return false;
	}

	/// Create window
	this->window = SDL_CreateWindow(
		this->appName.c_str(),
		static_cast<int>(this->width),
		static_cast<int>(this->height),
		SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
	);

	if (this->window == nullptr) {
		spdlog::error("Window could not be created! SDL_Error: {}", SDL_GetError());
		return false;
	}

	/// Initialize Vulkan
	this->vulkanInstance = std::make_unique<VulkanInstance>();

	/// Get required extensions from SDL
	Uint32 extensionCount = 0;
	const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);

	if (sdlExtensions == nullptr) {
		spdlog::error("Failed to get Vulkan extensions from SDL");
		return false;
	}

	/// Copy SDL extensions and add any additional required extensions
	std::vector<const char*> extensions(sdlExtensions, sdlExtensions + extensionCount);

	/// Add VK_EXT_debug_utils extension if you want to use validation layers
	extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

	if (!this->vulkanInstance->initialize(extensions)) {
		spdlog::error("Failed to initialize Vulkan instance");
		return false;
	}

	this->isRunning = true;
	return true;
}

void Application::run() {
	while (this->isRunning) {
		this->handleEvents();
		this->render();
	}
}

void Application::cleanup() {
	if (this->vulkanInstance) {
		this->vulkanInstance.reset();
	}

	if (this->window) {
		SDL_DestroyWindow(this->window);
		this->window = nullptr;
	}

	SDL_Quit();
}

void Application::handleEvents() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_EVENT_QUIT:
				this->isRunning = false;
				break;
			case SDL_EVENT_WINDOW_RESIZED:
				/// Handle window resize
				spdlog::info("Window resized to {}x{}", event.window.data1, event.window.data2);
				break;
			default:
				break;
		}
	}
}

void Application::render() {
	/// Placeholder for rendering code
	/// This will be implemented later when we set up the Vulkan rendering pipeline
}