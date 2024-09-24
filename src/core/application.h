#pragma once

#include "vulkan/vulkaninstance.h"
#include "vulkan/vulkandevice.h"
#include "vulkan/vulkanswapchain.h"
#include <SDL3/SDL.h>
#include <string>
#include <memory>

class Application {
public:
	Application(const std::string& appName, uint32_t width, uint32_t height);
	~Application();

	/// Initialize the application
	bool initialize();

	/// Run the main loop
	void run();

	/// Clean up resources
	void cleanup();

private:
	/// Handle SDL events
	void handleEvents();

	/// Perform rendering
	void render();

	/// Select a suitable physical device (GPU)
	VkPhysicalDevice pickPhysicalDevice();

	/// Create the swap chain
	bool createSwapChain();

	/// Recreate the swap chain
	bool recreateSwapChain();

	/// Clean up swap chain
	void cleanupSwapChain();

	std::string appName;
	uint32_t width;
	uint32_t height;

	SDL_Window* window;
	std::unique_ptr<VulkanInstance> vulkanInstance;
	std::unique_ptr<VulkanDevice> vulkanDevice;
	std::unique_ptr<VulkanSwapchain> vulkanSwapchain;
	VkPhysicalDevice physicalDevice;
	VkSurfaceKHR surface;
	bool isRunning;
	bool framebufferResized;
};