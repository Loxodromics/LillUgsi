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

	/// Create Vulkan surface
	if (!SDL_Vulkan_CreateSurface(this->window, this->vulkanInstance->getInstance(), nullptr, &this->surface)) {
		spdlog::error("Failed to create Vulkan surface");
		return false;
	}

	/// Create logical device
	this->vulkanDevice = std::make_unique<VulkanDevice>();
	std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	if (!this->vulkanDevice->initialize(this->physicalDevice, deviceExtensions)) {
		spdlog::error("Failed to create logical device");
		return false;
	}


	/// Create swap chain
	if (!this->createSwapChain()) {
		spdlog::error("Failed to create swap chain");
		return false;
	}

	this->isRunning = true;
	this->framebufferResized = false;
	return true;
}

void Application::run() {
	while (this->isRunning) {
		this->handleEvents();
		this->render();
	}
}

bool Application::createSwapChain() {
	this->vulkanSwapchain = std::make_unique<VulkanSwapchain>();
	return this->vulkanSwapchain->initialize(this->physicalDevice, this->vulkanDevice->getDevice(), this->surface, this->width, this->height);
}

bool Application::recreateSwapChain() {
	int w, h;
	SDL_GetWindowSizeInPixels(this->window, &w, &h);
	while (w == 0 || h == 0) {
		SDL_GetWindowSizeInPixels(this->window, &w, &h);
		SDL_WaitEvent(nullptr);
	}
	this->width = static_cast<uint32_t>(w);
	this->height = static_cast<uint32_t>(h);

	vkDeviceWaitIdle(this->vulkanDevice->getDevice());

	this->cleanupSwapChain();

	return this->createSwapChain();
}

void Application::cleanupSwapChain() {
	this->vulkanSwapchain.reset();
}

void Application::handleEvents() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_EVENT_QUIT:
				this->isRunning = false;
			break;
			case SDL_EVENT_WINDOW_RESIZED:
				/// This does not get called for me...
				this->framebufferResized = true;
			break;
			default:
				break;
		}
	}
}

void Application::render() {
	if (this->framebufferResized) {
		this->recreateSwapChain();
		this->framebufferResized = false;
		return;  // Skip this frame
	}

	/// Placeholder for rendering code
	/// This will be implemented later when we set up the Vulkan rendering pipeline
}

void Application::cleanup() {
	this->cleanupSwapChain();

	if (this->surface) {
		vkDestroySurfaceKHR(this->vulkanInstance->getInstance(), this->surface, nullptr);
		this->surface = VK_NULL_HANDLE;
	}

	this->vulkanDevice.reset();
	this->vulkanInstance.reset();

	if (this->window) {
		SDL_DestroyWindow(this->window);
		this->window = nullptr;
	}

	SDL_Quit();
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