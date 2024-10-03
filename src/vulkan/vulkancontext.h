#pragma once

#include "vulkan/vulkaninstance.h"
#include "vulkan/vulkandevice.h"
#include "vulkan/vulkanswapchain.h"
#include "vulkan/vulkanexception.h"
#include <SDL3/SDL.h>
#include <memory>
#include <vector>

namespace lillugsi::vulkan {

/// VulkanContext encapsulates the core Vulkan objects and initialization process.
/// This class is responsible for creating and managing the Vulkan instance, 
/// physical device selection, logical device creation, and swap chain setup.
/// By centralizing these core Vulkan objects, we improve code organization and
/// make it easier to manage the lifetime of these objects.
class VulkanContext {
public:
	VulkanContext();
	~VulkanContext();

	/// Initialize the Vulkan context
	/// This method sets up all the necessary Vulkan objects for rendering
	/// @param window The SDL window to create the Vulkan surface for
	/// @return True if initialization was successful, false otherwise
	bool initialize(SDL_Window* window);

	/// Clean up all Vulkan resources
	/// This method should be called before the VulkanContext is destroyed
	void cleanup();

	/// Get the Vulkan instance
	/// @return A pointer to the VulkanInstance object
	VulkanInstance* getInstance() const { return this->vulkanInstance.get(); }

	/// Get the Vulkan device
	/// @return A pointer to the VulkanDevice object
	VulkanDevice* getDevice() const { return this->vulkanDevice.get(); }

	/// Get the Vulkan swap chain
	/// @return A pointer to the VulkanSwapchain object
	VulkanSwapchain* getSwapChain() const { return this->vulkanSwapchain.get(); }

	/// Get the Vulkan surface
	/// @return The VkSurfaceKHR handle
	VkSurfaceKHR getSurface() const { return this->surface; }

	/// Get the physical device
	/// @return The VkPhysicalDevice handle
	VkPhysicalDevice getPhysicalDevice() const { return this->physicalDevice; }

	/// This method initializes the swap chain for rendering
	void createSwapChain(uint32_t width, uint32_t height);


private:
	/// This method creates the Vulkan instance with necessary extensions
	void initializeVulkan();

	/// Create the Vulkan surface for rendering
	/// This method creates a Vulkan surface from the SDL window
	/// @param window The SDL window to create the surface from
	void createSurface(SDL_Window* window);

	/// This method chooses the most appropriate physical device for rendering
	void pickPhysicalDevice();

	/// This method creates a logical device with the required queues and extensions
	void createLogicalDevice();

	std::unique_ptr<VulkanInstance> vulkanInstance;
	std::unique_ptr<VulkanDevice> vulkanDevice;
	std::unique_ptr<VulkanSwapchain> vulkanSwapchain;
	VkPhysicalDevice physicalDevice;
	VkSurfaceKHR surface;

	uint32_t width;
	uint32_t height;
};

} /// namespace lillugsi::vulkan