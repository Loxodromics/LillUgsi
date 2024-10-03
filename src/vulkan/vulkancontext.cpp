/// vulkancontext.cpp
#include "vulkancontext.h"
#include <SDL3/SDL_vulkan.h>
#include <spdlog/spdlog.h>
#include <set>

namespace lillugsi::vulkan {

VulkanContext::VulkanContext()
	: physicalDevice(VK_NULL_HANDLE)
	, surface(VK_NULL_HANDLE)
	, width(0)
	, height(0) {
}

VulkanContext::~VulkanContext() {
	this->cleanup();
}

bool VulkanContext::initialize(SDL_Window* window) {
	/// Get the window size for creating the swap chain
	SDL_GetWindowSizeInPixels(window, reinterpret_cast<int*>(&this->width), reinterpret_cast<int*>(&this->height));

	try {
		this->initializeVulkan();
		this->createSurface(window);
		this->pickPhysicalDevice();
		this->createLogicalDevice();
		this->createSwapChain(this->width, this->height);
		return true;
	}
	catch (const VulkanException& e) {
		spdlog::error("Vulkan error during initialization: {}", e.what());
		return false;
	}
	catch (const std::exception& e) {
		spdlog::error("Error during initialization: {}", e.what());
		return false;
	}
}

void VulkanContext::cleanup() {
	/// Clean up in reverse order of creation
	this->vulkanSwapchain.reset();

	if (this->vulkanInstance && this->surface) {
		vkDestroySurfaceKHR(this->vulkanInstance->getInstance(), this->surface, nullptr);
		this->surface = VK_NULL_HANDLE;
	}

	this->vulkanDevice.reset();
	this->vulkanInstance.reset();

	spdlog::info("VulkanContext cleanup completed");
}

void VulkanContext::initializeVulkan() {
	/// Initialize Vulkan instance
	this->vulkanInstance = std::make_unique<VulkanInstance>();

	/// Get required extensions from SDL
	Uint32 extensionCount = 0;
	const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);

	if (sdlExtensions == nullptr) {
		throw vulkan::VulkanException(VK_ERROR_EXTENSION_NOT_PRESENT, "Failed to get Vulkan extensions from SDL", __FUNCTION__, __FILE__, __LINE__);
	}

	/// Copy SDL extensions and add any additional required extensions
	std::vector<const char*> extensions(sdlExtensions, sdlExtensions + extensionCount);

	/// Add VK_EXT_debug_utils extension if you want to use validation layers
	extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

	/// Log available Vulkan extensions
	uint32_t availableExtensionCount;
	vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, nullptr);
	std::vector<VkExtensionProperties> availableExtensions(availableExtensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, availableExtensions.data());

	spdlog::info("Available Vulkan extensions:");
	for (const auto& extension : availableExtensions) {
		spdlog::info("  {}", extension.extensionName);
	}

	spdlog::info("Required Vulkan extensions:");
	for (const auto& extension : extensions) {
		spdlog::info("  {}", extension);
	}

	/// Initialize the Vulkan instance with the required extensions
	this->vulkanInstance->initialize(extensions);

	spdlog::info("Vulkan initialized successfully");
}

void VulkanContext::createSurface(SDL_Window* window) {
	/// Create Vulkan surface using SDL3's new method
	/// This creates a surface that bridges Vulkan and the platform's window system
	VkSurfaceKHR surface;
	if (!SDL_Vulkan_CreateSurface(window, this->vulkanInstance->getInstance(), nullptr, &surface)) {
		throw vulkan::VulkanException(VK_ERROR_INITIALIZATION_FAILED, "Failed to create Vulkan surface", __FUNCTION__, __FILE__, __LINE__);
	}
	this->surface = surface;
	spdlog::info("Vulkan surface created successfully");
}

void VulkanContext::pickPhysicalDevice() {
	/// Enumerate all available physical devices
	uint32_t deviceCount = 0;
	VK_CHECK(vkEnumeratePhysicalDevices(this->vulkanInstance->getInstance(), &deviceCount, nullptr));

	if (deviceCount == 0) {
		throw VulkanException(VK_ERROR_INITIALIZATION_FAILED, "Failed to find GPUs with Vulkan support", __FUNCTION__, __FILE__, __LINE__);
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	VK_CHECK(vkEnumeratePhysicalDevices(this->vulkanInstance->getInstance(), &deviceCount, devices.data()));

	/// TODO: Implement device selection logic
	/// For now, just pick the first device
	/// In a more sophisticated implementation, you might want to score devices based on their properties and features
	this->physicalDevice = devices[0];

	if (this->physicalDevice == VK_NULL_HANDLE) {
		throw VulkanException(VK_ERROR_INITIALIZATION_FAILED, "Failed to find a suitable GPU", __FUNCTION__, __FILE__, __LINE__);
	}

	spdlog::info("Physical device selected successfully");
}

void VulkanContext::createLogicalDevice() {
	this->vulkanDevice = std::make_unique<VulkanDevice>();

	/// Specify the extensions required for the logical device
	/// VK_KHR_SWAPCHAIN_EXTENSION_NAME is required for creating a swap chain
	/// "VK_KHR_portability_subset" is required on some platforms (e.g., macOS) for Vulkan compatibility
	std::vector<const char*> deviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		"VK_KHR_portability_subset"
	};

	/// Initialize the logical device with the selected physical device and required extensions
	this->vulkanDevice->initialize(this->physicalDevice, deviceExtensions);

	spdlog::info("Logical device created successfully");
}

void VulkanContext::createSwapChain(uint32_t width, uint32_t height) {
	this->vulkanSwapchain = std::make_unique<VulkanSwapchain>();

	/// Initialize the swap chain with the current window dimensions
	/// The swap chain is crucial for presenting rendered images to the screen
	this->vulkanSwapchain->initialize(this->physicalDevice, this->vulkanDevice->getDevice(), this->surface, width, height);

	spdlog::info("Swap chain created successfully");
}

}