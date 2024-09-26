#pragma once

#include "vulkan/vulkaninstance.h"
#include "vulkan/vulkandevice.h"
#include "vulkan/vulkanswapchain.h"
#include "vulkan/vulkanwrappers.h"
#include <SDL3/SDL.h>
#include <memory>
#include <vector>

class Renderer {
public:
	Renderer();
	~Renderer();

	/// Initialize the renderer with the given SDL window
	bool initialize(SDL_Window* window);

	/// Clean up all Vulkan resources
	void cleanup();

	/// Draw a frame
	void drawFrame();

	/// Recreate the swap chain (e.g., after window resize)
	bool recreateSwapChain(uint32_t newWidth, uint32_t newHeight);

	/// Check if the swap chain is compatible with the current window
	bool isSwapChainAdequate() const;

private:
	/// Initialize Vulkan-specific components
	bool initializeVulkan();

	/// Create the Vulkan surface for rendering
	bool createSurface(SDL_Window* window);

	/// Select a suitable physical device (GPU)
	VkPhysicalDevice pickPhysicalDevice();

	/// Create the swap chain
	bool createSwapChain();

	/// Create the command pool
	bool createCommandPool();

	/// Create command buffers
	bool createCommandBuffers();

	/// Create the render pass
	bool createRenderPass();

	/// Create framebuffers for each swap chain image
	bool createFramebuffers();

	/// Clean up existing framebuffers
	void cleanupFramebuffers();

	std::unique_ptr<VulkanInstance> vulkanInstance;
	std::unique_ptr<VulkanDevice> vulkanDevice;
	std::unique_ptr<VulkanSwapchain> vulkanSwapchain;
	VkPhysicalDevice physicalDevice;
	VkSurfaceKHR surface;
	VulkanCommandPoolHandle commandPool; /// Command pool for allocating command buffers
	std::vector<VkCommandBuffer> commandBuffers; /// Command buffers for recording drawing commands
	VulkanRenderPassHandle renderPass; /// Handle for the render pass
	std::vector<VulkanFramebufferHandle> swapChainFramebuffers; /// Vector to store framebuffer handles

	uint32_t width;
	uint32_t height;

	bool isCleanedUp;
};