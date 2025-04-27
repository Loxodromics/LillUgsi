#pragma once

#include "vulkan/vulkancontext.h"
#include "vulkan/vulkanbuffer.h"
#include "vulkan/vulkanwrappers.h"
#include "vulkan/pipelinemanager.h"
#include "vulkan/commandbuffermanager.h"
#include "vulkan/framebuffermanager.h"
#include "vulkan/depthbuffer.h"
#include "rendering/editorcamera.h"
#include "rendering/orbitcamera.h"
#include "rendering/meshmanager.h"
#include "rendering/lightmanager.h"
#include "rendering/texturemanager.h"
#include "rendering/screenshot.h"
#include "scene/scene.h"
#include "materialmanager.h"
#include "buffermanager.h"
#include "models/modelmanager.h"
#include "pipelinefactory.h"
#include "models/materialparametermapper.h"
#include "models/textureloadingpipeline.h"


#ifdef USE_PLANET
#include "planet/planetdata.h"
#endif


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
	Camera* getCamera() { return this->camera.get(); }

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

	/// Load a model from file and create necessary pipelines
	/// This is a high-level method that coordinates the model loading process:
	/// 1. Load the model file and create scene nodes
	/// 2. Wait for textures to load
	/// 3. Create pipelines for materials
	/// 4. Update bounds for culling
	/// @param filePath Path to the model file
	/// @param parentNode Parent node to attach the model to (optional)
	/// @return Root node of the loaded model, or nullptr if loading failed
	[[nodiscard]] std::shared_ptr<scene::SceneNode> loadModel(
		const std::string& filePath,
		std::shared_ptr<scene::SceneNode> parentNode);

	/// Load a model asynchronously from file
	/// This starts a background task to load the model without blocking the main thread
	/// @param filePath Path to the model file
	/// @param parentNode Parent node to attach the model to (optional)
	/// @return Future that will contain the root node when loading completes
	[[nodiscard]] std::future<std::shared_ptr<scene::SceneNode>> loadModelAsync(
		const std::string& filePath,
		std::shared_ptr<scene::SceneNode> parentNode = nullptr);

	/// Capture the current frame as a screenshot
	/// @param filename The name of the file to save (PNG format)
	/// @return True if the screenshot was saved successfully
	bool captureScreenshot(const std::string& filename);

private:
	/// Struct to hold camera data for GPU
	struct CameraUBO {
		glm::mat4 view;
		glm::mat4 projection;
		glm::vec3 cameraPos;  /// Camera position for view direction calculations
		float padding;        /// Padding to ensure proper alignment (vec3 needs to be padded to vec4)
	};

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
	void initializeModelManager();
	/// Sets up the material mapper, texture loader, and pipeline factory
	void initializeModelLoadingComponents();

	/// Vulkan context managing Vulkan instance, device, and swap chain
	std::unique_ptr<vulkan::VulkanContext> vulkanContext;

	/// Command buffers for recording drawing commands
	std::vector<VkCommandBuffer> commandBuffers;

	/// Command pool for rendering operations
	/// Raw handle owned by CommandBufferManager
	VkCommandPool commandPool = VK_NULL_HANDLE;

	/// Handle for the render pass
	vulkan::VulkanRenderPassHandle renderPass;

	/// Vulkan buffer utility
	std::unique_ptr<vulkan::VulkanBuffer> vulkanBufferUtility;

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
	std::shared_ptr<vulkan::PipelineManager> pipelineManager;

	/// MeshManager for creating and managing meshes
	std::shared_ptr<MeshManager> meshManager;

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
	// std::unique_ptr<EditorCamera> camera;
	std::unique_ptr<OrbitCamera> camera;

	/// Flag to track if cleanup has been performed
	bool isCleanedUp;

	/// Tracks time since last frame for animations and effects
	float currentFrameTime{0.0f};

	/// Light management
	std::unique_ptr<LightManager> lightManager;

	/// Material management
	std::shared_ptr<MaterialManager> materialManager;

#ifdef USE_PLANET
	std::shared_ptr<planet::PlanetData> icosphere;
#endif

	std::shared_ptr<scene::SceneNode> texturedCubeNode;

	/// Screenshot manager variables
	std::unique_ptr<Screenshot> screenshotManager;
	uint32_t lastPresentedImageIndex = 0;

	/// Texture Manager
	std::shared_ptr<rendering::TextureManager> textureManager;

	/// Command buffer manager for centralized command buffer operations
	std::shared_ptr<vulkan::CommandBufferManager> commandBufferManager;

	/// Add framebuffer manager
	std::unique_ptr<vulkan::FramebufferManager> framebufferManager;

	std::shared_ptr<BufferManager> bufferManager;
	std::shared_ptr<vulkan::Buffer> cameraBuffer;
	std::shared_ptr<vulkan::Buffer> lightBuffer;
	std::unique_ptr<ModelManager> modelManager;

	/// Pipeline factory for model material pipelines
	/// This centralizes the creation of specialized rendering pipelines
	/// based on the materials used by loaded models
	std::unique_ptr<PipelineFactory> pipelineFactory;

	/// Material parameter mapper for model materials
	/// This converts material data from model formats to our engine format,
	/// ensuring consistent material parameter handling across different models
	std::unique_ptr<models::MaterialParameterMapper> materialMapper;

	/// Texture loading pipeline for model textures
	/// This provides asynchronous loading and texture processing
	/// to prevent blocking the main thread during model loading
	std::unique_ptr<TextureLoadingPipeline> textureLoader;

};

} /// namespace lillugsi::rendering