#include "application.h"
#include <spdlog/spdlog.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <vector>

Application::Application(const std::string& appName, uint32_t width, uint32_t height)
	: appName(appName), width(width), height(height), window(nullptr),
	physicalDevice(VK_NULL_HANDLE), isRunning(false) {
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

	/// Log available Vulkan extensions
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

	spdlog::info("Available Vulkan extensions:");
	for (const auto& extension : availableExtensions) {
		spdlog::info("  {}", extension.extensionName);
	}

	spdlog::info("Required Vulkan extensions:");
	for (const auto& extension : extensions) {
		spdlog::info("  {}", extension);
	}

	if (!this->vulkanInstance->initialize(extensions)) {
		spdlog::error("Failed to initialize Vulkan instance: {}", this->vulkanInstance->getLastError());
		return false;
	}

	/// Select a physical device
	this->physicalDevice = this->pickPhysicalDevice();
	if (this->physicalDevice == VK_NULL_HANDLE) {
		spdlog::error("Failed to find a suitable GPU");
		return false;
	}

	/// Create logical device
	this->vulkanDevice = std::make_unique<VulkanDevice>();
	std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	if (!this->vulkanDevice->initialize(this->physicalDevice, deviceExtensions)) {
		spdlog::error("Failed to create logical device");
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
	this->vulkanDevice.reset();
	this->vulkanInstance.reset();

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

VkPhysicalDevice Application::pickPhysicalDevice() {
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(this->vulkanInstance->getInstance(), &deviceCount, nullptr);

	if (deviceCount == 0) {
		spdlog::error("Failed to find GPUs with Vulkan support");
		return VK_NULL_HANDLE;
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(this->vulkanInstance->getInstance(), &deviceCount, devices.data());

	/// For now, just pick the first device
	/// In a real application, you'd want to score and rank the devices based on their properties
	return devices[0];
}