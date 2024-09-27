#pragma once

#include "vulkan/vulkaninstance.h"
#include "vulkan/vulkanbuffer.h"
#include "vulkan/vulkandevice.h"
#include "vulkan/vulkanswapchain.h"
#include "vulkan/vulkanwrappers.h"
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <vector>

class Renderer {
public:
	Renderer();
	~Renderer();

	/// Create and initialize the VulkanDevice
	void createLogicalDevice();

	/// Initialize the renderer with the given SDL window
	bool initialize(SDL_Window* window);

	/// Clean up all Vulkan resources
	void cleanup();

	/// Draw a frame
	void drawFrame();

	/// Recreate the swap chain (e.g., after window resize)
	bool recreateSwapChain(uint32_t newWidth, uint32_t newHeight);

private:
	/// Struct to hold camera data for GPU
	struct CameraUBO {
		glm::mat4 view;
		glm::mat4 projection;
	};

	/// Initialize Vulkan-specific components
	void initializeVulkan();

	/// Create the Vulkan surface for rendering
	void createSurface(SDL_Window* window);

	/// Select a suitable physical device (GPU)
	VkPhysicalDevice pickPhysicalDevice();

	/// Create the swap chain
	void createSwapChain();

	/// Create the command pool
	void createCommandPool();

	/// Create command buffers
	void createCommandBuffers();

	/// Create the render pass
	void createRenderPass();

	/// Create framebuffers for each swap chain image
	void createFramebuffers();

	/// Clean up existing framebuffers
	void cleanupFramebuffers();

	/// Create shader modules
	VulkanShaderModuleHandle createShaderModule(const std::vector<char>& code);

	/// Set up the graphics pipeline
	void createGraphicsPipeline();

	/// Record command buffers
	void recordCommandBuffers();

	/// Buffer to hold camera data
	VulkanBufferHandle cameraBuffer;

	/// Memory for the camera buffer
	VkDeviceMemory cameraBufferMemory;

	/// Create the camera uniform buffer
	void createCameraUniformBuffer();

	/// Update the camera uniform buffer with current camera data
	void updateCameraUniformBuffer();
	std::unique_ptr<VulkanInstance> vulkanInstance;
	std::unique_ptr<VulkanDevice> vulkanDevice;
	std::unique_ptr<VulkanSwapchain> vulkanSwapchain;
	VkPhysicalDevice physicalDevice;
	VkSurfaceKHR surface;
	VulkanCommandPoolHandle commandPool; /// Command pool for allocating command buffers
	std::vector<VkCommandBuffer> commandBuffers; /// Command buffers for recording drawing commands
	VulkanRenderPassHandle renderPass; /// Handle for the render pass
	std::vector<VulkanFramebufferHandle> swapChainFramebuffers; /// Vector to store framebuffer handles
	std::unique_ptr<VulkanBuffer> vulkanBuffer;

	/// Graphics pipeline
	VulkanPipelineLayoutHandle pipelineLayout;
	VulkanPipelineHandle graphicsPipeline;

	uint32_t width;
	uint32_t height;

	bool isCleanedUp;

};