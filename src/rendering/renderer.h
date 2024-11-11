/// renderer.h
#pragma once

#include "vulkan/vulkancontext.h"
#include "vulkan/vulkanbuffer.h"
#include "vulkan/vulkanwrappers.h"
#include "vulkan/pipelinemanager.h"
#include "vulkan/depthbuffer.h"
#include "rendering/editorcamera.h"
#include "rendering/mesh.h"
#include "rendering/meshmanager.h"
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace lillugsi::rendering {

/// Main renderer class responsible for managing the rendering pipeline
/// Depth testing configuration notes:
/// This renderer uses Reverse-Z depth buffering for improved precision
/// Key aspects of Reverse-Z:
/// 1. Depth range is [1,0] instead of [0,1]
/// 2. Near plane is at depth 1.0
/// 3. Far plane is at depth 0.0
/// 4. Depth comparison uses GREATER instead of LESS
/// 5. Depth buffer is cleared to 0.0
///
/// Benefits:
/// - Better floating-point precision for distant objects
/// - Reduced Z-fighting artifacts
/// - More natural distribution of depth precision
///
/// Implementation details:
/// - Projection matrix swaps near/far planes
/// - Vertex shader inverts Z component
/// - Pipeline uses VK_COMPARE_OP_GREATER
/// - Depth buffer cleared to 0.0

class Renderer {
public:
	/// Constructor
	Renderer();

	/// Destructor
	~Renderer();

	/// Initialize the renderer with the given SDL window
	/// @param window Pointer to the SDL window
	/// @return True if initialization was successful, false otherwise
	bool initialize(SDL_Window* window);

	/// Clean up all Vulkan resources
	void cleanup();

	/// Draw a frame
	void drawFrame();

	/// Recreate the swap chain (e.g., after window resize)
	/// @param newWidth New width of the window
	/// @param newHeight New height of the window
	/// @return True if swap chain recreation was successful, false otherwise
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

	void createCommandPool();
	void createCommandBuffers();
	void createRenderPass();
	void createFramebuffers();
	void cleanupFramebuffers();
	vulkan::VulkanShaderModuleHandle createShaderModule(const std::vector<char>& code);
	void createGraphicsPipeline();
	void recordCommandBuffers();
	void createCameraUniformBuffer();
	void createIndexBuffer();
	void createVertexBuffer();
	void updateCameraUniformBuffer();
	void initializeGeometry();
	void createDescriptorSetLayout();
	void createDescriptorPool();
	void createDescriptorSets();
	void createSyncObjects();
	void cleanupSyncObjects();
	void addMesh(std::unique_ptr<Mesh> mesh); /// Add a mesh to the scene
	void createMeshBuffers();
	void initializeDepthBuffer();

	/// Vulkan context managing Vulkan instance, device, and swap chain
	std::unique_ptr<vulkan::VulkanContext> vulkanContext;

	/// Command pool for allocating command buffers
	vulkan::VulkanCommandPoolHandle commandPool;

	/// Command buffers for recording drawing commands
	std::vector<VkCommandBuffer> commandBuffers;

	/// Handle for the render pass
	vulkan::VulkanRenderPassHandle renderPass;

	/// Vector to store framebuffer handles
	std::vector<vulkan::VulkanFramebufferHandle> swapChainFramebuffers;

	/// Vulkan buffer utility
	std::unique_ptr<vulkan::VulkanBuffer> vulkanBuffer;

	/// Buffer to hold camera uniform data
	vulkan::VulkanBufferHandle cameraBuffer;
	VkDeviceMemory cameraBufferMemory;

	/// Descriptor set layout
	VkDescriptorSetLayout descriptorSetLayout;

	/// Descriptor pool
	VkDescriptorPool descriptorPool;

	/// Descriptor sets
	std::vector<VkDescriptorSet> descriptorSets;

	/// Synchronization objects
	VkSemaphore imageAvailableSemaphore;
	VkSemaphore renderFinishedSemaphore;
	VkFence inFlightFence;

	/// Graphics pipeline
	std::shared_ptr<vulkan::VulkanPipelineHandle> graphicsPipeline;
	std::shared_ptr<vulkan::VulkanPipelineLayoutHandle> pipelineLayout;
	std::unique_ptr<vulkan::PipelineManager> pipelineManager;

	/// MeshManager for creating and managing meshes
	std::unique_ptr<MeshManager> meshManager;

	/// Vector to store meshes
	std::vector<std::unique_ptr<Mesh>> meshes;

	/// Depth buffer for z-testing
	/// This allows for proper rendering of 3D scenes by ensuring objects are drawn in the correct order
	std::unique_ptr<vulkan::DepthBuffer> depthBuffer;

	/// Window dimensions
	uint32_t width;
	uint32_t height;

	/// The camera used for rendering the scene
	/// We use a unique_ptr for automatic memory management and to allow for easy replacement if needed
	std::unique_ptr<EditorCamera> camera;

	/// Flag to track if cleanup has been performed
	bool isCleanedUp;
};

}