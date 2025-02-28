#pragma once

#include "vulkan/vulkancontext.h"
#include "vulkan/vulkanbuffer.h"
#include "vulkan/vulkanwrappers.h"
#include "vulkan/pipelinemanager.h"
#include "vulkan/depthbuffer.h"
#include "rendering/editorcamera.h"
#include "rendering/meshmanager.h"
#include "rendering/lightmanager.h"
#include "rendering/screenshot.h"
#include "scene/scene.h"
#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "materialmanager.h"

#include <planet/planetdata.h>

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

	/// Update the renderer state
	/// @param deltaTime Time elapsed since last frame, scaled by game time settings
	void update(float deltaTime);

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
	void handleCameraInput(SDL_Window* window, const SDL_Event& event) const;

	/// Get the scene manager
	/// This allows external systems to manipulate the scene
	/// @return A pointer to the Scene
	scene::Scene* getScene() { return this->scene.get(); }

	/// Get the light manager instance
	/// This allows external systems to add and modify lights
	/// @return Pointer to the light manager
	[[nodiscard]] LightManager* getLightManager() const { return this->lightManager.get(); }

	/// Get the material manager
	/// This allows external systems to create and modify materials
	/// @return Pointer to the material manager
	[[nodiscard]] MaterialManager* getMaterialManager() const {
		return this->materialManager.get();
	}

	/// Capture the current frame as a screenshot
	/// @param filename The name of the file to save (PNG format)
	/// @return True if the screenshot was saved successfully
	bool captureScreenshot(const std::string& filename);

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
	void updateCameraUniformBuffer() const;
	void createDescriptorPool();
	void createDescriptorSets();
	void createSyncObjects();
	void cleanupSyncObjects();
	void initializeDepthBuffer();
	void initializeScene();
	void createLightUniformBuffer();
	void updateLightUniformBuffer() const;
	void initializeMaterials();

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
	std::unique_ptr<vulkan::VulkanBuffer> vulkanBufferUtility;

	/// Buffer to hold camera uniform data
	vulkan::VulkanBufferHandle cameraBuffer;
	VkDeviceMemory cameraBufferMemory;

	/// Descriptor pool
	VkDescriptorPool descriptorPool;

	/// Descriptor sets
	std::vector<VkDescriptorSet> cameraDescriptorSets;  /// Set = 0 for camera data
	std::vector<VkDescriptorSet> lightDescriptorSets;   /// Set = 1 for light data

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

	/// Scene management
	std::unique_ptr<scene::Scene> scene;  /// Scene graph for object management

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

	/// Tracks time since last frame for animations and effects
	float currentFrameTime{0.0f};

	/// Light management
	std::unique_ptr<LightManager> lightManager;
	vulkan::VulkanBufferHandle lightBuffer;
	VkDeviceMemory lightBufferMemory;

	/// Material management
	std::unique_ptr<MaterialManager> materialManager;

	std::shared_ptr<planet::PlanetData> icosphere;

	/// Screenshot manager variables
	std::unique_ptr<Screenshot> screenshotManager;
	uint32_t lastPresentedImageIndex = 0;

};

} /// namespace lillugsi::rendering