#include "renderer.h"
#include <spdlog/spdlog.h>
#include <SDL3/SDL_vulkan.h>

Renderer::Renderer()
	: physicalDevice(VK_NULL_HANDLE)
	, surface(VK_NULL_HANDLE)
	, width(0)
	, height(0) {
}

Renderer::~Renderer() {
	this->cleanup();
}

bool Renderer::initialize(SDL_Window* window) {
	SDL_GetWindowSizeInPixels(window, reinterpret_cast<int*>(&this->width), reinterpret_cast<int*>(&this->height));

	if (!this->initializeVulkan()) {
		return false;
	}

	if (!this->createSurface(window)) {
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

	if (!this->createSwapChain()) {
		spdlog::error("Failed to create swap chain");
		return false;
	}

	return true;
}

void Renderer::cleanup() {
	this->vulkanSwapchain.reset();

	if (this->surface) {
		vkDestroySurfaceKHR(this->vulkanInstance->getInstance(), this->surface, nullptr);
		this->surface = VK_NULL_HANDLE;
	}

	this->vulkanDevice.reset();
	this->vulkanInstance.reset();
}

void Renderer::drawFrame() {
	/// TODO: Implement actual frame drawing
	/// This will be expanded in future tasks
}

bool Renderer::recreateSwapChain(uint32_t newWidth, uint32_t newHeight) {
	spdlog::info("Renderer::recreateSwapChain");
	vkDeviceWaitIdle(this->vulkanDevice->getDevice());

	this->vulkanSwapchain.reset();

	this->width = newWidth;
	this->height = newHeight;

	return this->createSwapChain();
}

bool Renderer::isSwapChainAdequate() const {
	/// TODO: Implement proper swap chain adequacy check
	/// This will be expanded in future tasks
	return this->vulkanSwapchain != nullptr;
}

bool Renderer::initializeVulkan() {
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

	return true;
}

bool Renderer::createSurface(SDL_Window* window) {
	if (!SDL_Vulkan_CreateSurface(window, this->vulkanInstance->getInstance(), nullptr, &this->surface)) {
		spdlog::error("Failed to create Vulkan surface");
		return false;
	}
	return true;
}

VkPhysicalDevice Renderer::pickPhysicalDevice() {
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(this->vulkanInstance->getInstance(), &deviceCount, nullptr);

	if (deviceCount == 0) {
		spdlog::error("Failed to find GPUs with Vulkan support");
		return VK_NULL_HANDLE;
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(this->vulkanInstance->getInstance(), &deviceCount, devices.data());

	/// TODO: Implement device selection logic
	/// For now, just pick the first device
	return devices[0];
}

bool Renderer::createSwapChain() {
	this->vulkanSwapchain = std::make_unique<VulkanSwapchain>();
	return this->vulkanSwapchain->initialize(this->physicalDevice, this->vulkanDevice->getDevice(), this->surface, this->width, this->height);
}