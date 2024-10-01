#pragma once

#include "vulkan/vulkaninstance.h"
#include "vulkan/vulkanbuffer.h"
#include "vulkan/vulkandevice.h"
#include "vulkan/vulkanswapchain.h"
#include "vulkan/vulkanwrappers.h"
#include "rendering/editorcamera.h"
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <vector>

struct Vertex {
	glm::vec3 pos;
	glm::vec3 color;
};

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

	/// Get a pointer to the camera
	/// This allows other parts of the application to interact with the camera
	/// @return A pointer to the EditorCamera
	EditorCamera* getCamera() { return this->camera.get(); }

	/// Handle input events for the camera
	/// This method should be called for each relevant SDL event
	/// @param window The SDL window, needed for mouse capture
	/// @param event The SDL event to process
	void handleCameraInput(SDL_Window* window, const SDL_Event& event);

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

	/// Create the camera uniform buffer
	void createCameraUniformBuffer();

	void createIndexBuffer();
	void createVertexBuffer();

	/// Update the camera uniform buffer with current camera data
	void updateCameraUniformBuffer();
	void initializeGeometry();

	void createDescriptorSetLayout();
	void createDescriptorPool();
	void createDescriptorSets();

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
	VulkanBufferHandle vertexBuffer;
	VkDeviceMemory vertexBufferMemory;
	VulkanBufferHandle indexBuffer;
	VkDeviceMemory indexBufferMemory;
	std::vector<Vertex> vertices;
	VulkanBufferHandle cameraBuffer; 	/// Buffer to hold camera uniform data
	VkDeviceMemory cameraBufferMemory;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorPool descriptorPool;
	std::vector<VkDescriptorSet> descriptorSets;

	/// Index data
	std::vector<uint16_t> indices;
	/// Graphics pipeline
	VulkanPipelineLayoutHandle pipelineLayout;
	VulkanPipelineHandle graphicsPipeline;

	uint32_t width;
	uint32_t height;

	/// The camera used for rendering the scene
	/// We use a unique_ptr for automatic memory management and to allow for easy replacement if needed
	std::unique_ptr<EditorCamera> camera;

	bool isCleanedUp;

};