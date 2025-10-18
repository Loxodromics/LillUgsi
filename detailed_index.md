# LillUgsi Detailed Index

This file contains detailed information about all classes, structs, and key functions in the LillUgsi renderer, organized by file.

---

## src/core/application.h

### Struct: `GameTime`
Time management structure separating game time from real time.

**Members:**
- `float deltaTime` - Time elapsed since last frame
- `float totalTime` - Total running time
- `float timeScale` - Scale factor for time (1.0 = normal)
- `bool isPaused` - Pause state
- `static constexpr float fixedTimeStep` - Fixed time step for physics (1/60)

### Class: `Application`
Main application class managing game loop and window lifecycle.

**Public Methods:**
- `Application(const std::string& appName, uint32_t width, uint32_t height)` - Constructor with window configuration
- `~Application()` - Destructor
- `bool initialize()` - Initialize the application
- `void run()` - Run the main game loop
- `void cleanup()` - Clean up resources
- `const GameTime& getGameTime() const` - Get current game time information
- `void setTimeLogInterval(float interval)` - Set time logging interval
- `void setMaxDeltaTime(float maxDelta)` - Set maximum allowed delta time

**Protected Methods:**
- `void handleEvents()` - Process SDL events and update application state
- `void handleCameraInput(const SDL_Event& event)` - Delegate camera input to renderer
- `void update()` - Update game state with delta time
- `void fixedUpdate()` - Perform fixed time step updates
- `void render()` - Perform rendering
- `void updateTime()` - Manage frame timing, scaling, and fixed time step
- `void takeScreenshot() const` - Capture and save screenshot

---

## src/main.cpp

### Function: `main(int argc, char* argv[])`
Entry point for the LillUgsi application.

**Responsibilities:**
- Initialize spdlog logging
- Create and initialize Application instance
- Run main loop
- Handle exceptions and return appropriate exit codes

---

## src/vulkan/vulkancontext.h

### Class: `VulkanContext`
Encapsulates core Vulkan objects and initialization.

**Public Methods:**
- `VulkanContext()` - Constructor
- `~VulkanContext()` - Destructor
- `bool initialize(SDL_Window* window)` - Initialize Vulkan with SDL window
- `void cleanup()` - Clean up Vulkan resources
- `VulkanInstance* getInstance() const` - Get Vulkan instance
- `VulkanDevice* getDevice() const` - Get Vulkan device
- `VulkanSwapchain* getSwapChain() const` - Get swap chain
- `VkSurfaceKHR getSurface() const` - Get Vulkan surface
- `VkPhysicalDevice getPhysicalDevice() const` - Get physical device
- `void createSwapChain(uint32_t width, uint32_t height)` - Initialize swap chain

**Private Methods:**
- `void initializeVulkan()` - Create Vulkan instance with extensions
- `void createSurface(SDL_Window* window)` - Create surface from SDL window
- `void pickPhysicalDevice()` - Choose appropriate physical device
- `void createLogicalDevice()` - Create logical device with queues

---

## src/vulkan/vulkaninstance.h

### Class: `VulkanInstance`
Manages Vulkan instance creation and validation layers.

**Public Methods:**
- `VulkanInstance()` - Constructor
- `~VulkanInstance()` - Destructor (default)
- `bool initialize(const std::vector<const char*>& requiredExtensions)` - Initialize instance
- `VkInstance getInstance() const` - Get instance handle
- `const std::string& getLastError() const` - Get last error message

**Private Methods:**
- `void setupDebugMessenger()` - Configure validation layer debug output
- `bool checkValidationLayerSupport()` - Verify validation layers are available

---

## src/vulkan/vulkandevice.h

### Class: `VulkanDevice`
Manages logical device and queue creation.

**Public Methods:**
- `VulkanDevice()` - Constructor
- `~VulkanDevice()` - Destructor (default)
- `void initialize(VkPhysicalDevice physicalDevice, const std::vector<const char*>& requiredExtensions)` - Initialize device
- `VkDevice getDevice() const` - Get logical device handle
- `VkQueue getGraphicsQueue() const` - Get graphics queue
- `VkQueue getPresentQueue() const` - Get present queue
- `uint32_t getGraphicsQueueFamilyIndex() const` - Get graphics queue family index

**Private Methods:**
- `void findQueueFamilies(VkPhysicalDevice, uint32_t& graphicsFamily, uint32_t& presentFamily)` - Find queue families
- `void createLogicalDevice(VkPhysicalDevice, uint32_t graphicsFamily, uint32_t presentFamily, const std::vector<const char*>&)` - Create device

---

## src/vulkan/vulkanswapchain.h

### Class: `VulkanSwapchain`
Manages swap chain creation and image views.

**Public Methods:**
- `VulkanSwapchain()` - Constructor
- `~VulkanSwapchain()` - Destructor (default)
- `void initialize(VkPhysicalDevice, VkDevice, VkSurfaceKHR, uint32_t width, uint32_t height)` - Initialize swap chain
- `VkSwapchainKHR getSwapChain() const` - Get swap chain handle
- `const std::vector<VkImage>& getSwapChainImages() const` - Get swap chain images
- `const std::vector<VulkanImageViewHandle>& getSwapChainImageViews() const` - Get image views
- `VkFormat getSwapChainImageFormat() const` - Get image format
- `VkExtent2D getSwapChainExtent() const` - Get swap extent

**Private Methods:**
- `VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>&)` - Choose optimal format
- `VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>&)` - Choose present mode
- `VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR&, uint32_t width, uint32_t height)` - Choose extent
- `void createImageViews(VkDevice device)` - Create image views for swap chain

---

## src/vulkan/vulkanbuffer.h

### Class: `VulkanBuffer`
Generic GPU buffer creation and copy operations.

**Public Methods:**
- `VulkanBuffer(VkDevice device, VkPhysicalDevice physicalDevice)` - Constructor
- `~VulkanBuffer()` - Destructor (default)
- `void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VulkanBufferHandle& buffer, VkDeviceMemory& bufferMemory)` - Create buffer
- `void copyBuffer(VkCommandPool, VkQueue, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, CommandBufferManager*)` - Copy between buffers

**Private Methods:**
- `uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)` - Find memory type

---

## src/vulkan/buffer.h

### Class: `Buffer`
Base RAII buffer wrapper (non-copyable, movable).

**Public Methods:**
- `Buffer(VkDevice, VkDeviceMemory, VulkanBufferHandle buffer, VkDeviceSize size, VkBufferUsageFlags usage)` - Constructor
- `VkBuffer get() const` - Get VkBuffer handle
- `VkDeviceSize getSize() const` - Get buffer size
- `VkBufferUsageFlags getUsage() const` - Get usage flags
- `void* map()` - Map buffer to CPU memory
- `void unmap()` - Unmap buffer
- `void update(const void* data, VkDeviceSize size, VkDeviceSize offset = 0)` - Map, copy, unmap convenience

---

## src/vulkan/vertexbuffer.h

### Class: `VertexBuffer : Buffer`
Specialized vertex buffer with vertex count tracking.

**Public Methods:**
- `VertexBuffer(VkDevice, VkDeviceMemory, VulkanBufferHandle, VkDeviceSize size, uint32_t vertexCount, uint32_t stride, VkBufferUsageFlags usage)` - Constructor
- `uint32_t getVertexCount() const` - Get vertex count
- `uint32_t getStride() const` - Get vertex stride
- `template<typename T> void updateVertices(const std::vector<T>& vertices)` - Type-safe vertex update

---

## src/vulkan/indexbuffer.h

### Class: `IndexBuffer : Buffer`
Specialized index buffer with index type management.

**Public Methods:**
- `IndexBuffer(VkDevice, VkDeviceMemory, VulkanBufferHandle, VkDeviceSize size, uint32_t indexCount, VkIndexType indexType, VkBufferUsageFlags usage)` - Constructor
- `uint32_t getIndexCount() const` - Get index count
- `VkIndexType getIndexType() const` - Get index type (16/32-bit)
- `template<typename T> void updateIndices(const std::vector<T>& indices)` - Type-safe index update

---

## src/vulkan/depthbuffer.h

### Class: `DepthBuffer`
Depth buffer creation and format selection.

**Public Methods:**
- `DepthBuffer(VkDevice device, VkPhysicalDevice physicalDevice)` - Constructor
- `~DepthBuffer()` - Destructor
- `void initialize(uint32_t width, uint32_t height)` - Initialize depth buffer
- `VkImageView getImageView() const` - Get image view
- `VkFormat getFormat() const` - Get depth format

**Private Methods:**
- `void cleanup()` - Clean up resources
- `VkFormat findSupportedFormat()` - Find suitable depth format
- `bool hasStencilComponent(VkFormat format)` - Check for stencil component
- `uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)` - Find memory type

---

## src/vulkan/pipelinemanager.h

### Struct: `PipelineManager::PipelineCache`
Cache structure for shared pipeline resources.

**Members:**
- `VkPipeline pipeline` - Raw pipeline handle for sharing
- `VkPipelineLayout layout` - Raw layout handle
- `uint32_t referenceCount` - Track materials using this pipeline

### Struct: `PipelineManager::MaterialPipeline`
RAII handles for material-specific pipelines.

**Members:**
- `std::shared_ptr<VulkanPipelineHandle> pipeline` - Pipeline handle
- `std::shared_ptr<VulkanPipelineLayoutHandle> layout` - Layout handle

### Class: `PipelineManager`
Manages graphics pipeline creation and caching.

**Public Methods:**
- `PipelineManager(VkDevice device, VkRenderPass renderPass)` - Constructor
- `~PipelineManager()` - Destructor (default)
- `void initialize()` - Initialize global descriptor layouts
- `std::shared_ptr<VulkanPipelineHandle> createPipeline(const rendering::Material& material)` - Create pipeline for material
- `std::shared_ptr<VulkanPipelineHandle> getPipeline(const std::string& name)` - Get pipeline by name
- `std::shared_ptr<VulkanPipelineLayoutHandle> getPipelineLayout(const std::string& name) const` - Get layout by name
- `VkDescriptorSetLayout getCameraDescriptorLayout() const` - Get camera descriptor layout (set = 0)
- `VkDescriptorSetLayout getLightDescriptorLayout() const` - Get light descriptor layout (set = 1)
- `bool hasPipeline(const std::string& materialName) const` - Check pipeline existence
- `void cleanup()` - Clean up all pipelines

**Private Methods:**
- `std::shared_ptr<ShaderProgram> createShaderProgram(const rendering::ShaderPaths& paths)` - Create shader program
- `std::shared_ptr<ShaderProgram> getOrCreateShaderProgram(const rendering::ShaderPaths& paths)` - Get or create with caching
- `static std::string generateShaderKey(const rendering::ShaderPaths& paths)` - Generate cache key
- `void createGlobalDescriptorLayouts()` - Create global descriptor layouts
- `MaterialPipeline getOrCreatePipeline(PipelineConfig& config, const rendering::Material& material)` - Get or create pipeline

---

## src/vulkan/pipelineconfig.h

### Struct: `PipelineShaderStage`
Shader stage configuration.

**Members:**
- `VkShaderStageFlagBits stage` - Shader stage type
- `std::string path` - Path to SPIR-V file
- `std::string entryPoint` - Entry point function name

### Class: `PipelineConfig`
Graphics pipeline configuration builder.

**Public Methods:**
- `void addShaderStage(VkShaderStageFlagBits stage, const std::string& path, const std::string& entryPoint = "main")` - Add shader
- `void setVertexInput(const VkVertexInputBindingDescription& binding, const std::vector<VkVertexInputAttributeDescription>& attributes)` - Set vertex input
- `void setInputAssembly(VkPrimitiveTopology topology, VkBool32 primitiveRestartEnable = VK_FALSE)` - Set topology
- `void setRasterization(VkPolygonMode polygonMode, VkCullModeFlags cullMode, VkFrontFace frontFace)` - Set rasterization
- `void setDepthState(VkBool32 depthTestEnable, VkBool32 depthWriteEnable, VkCompareOp depthCompareOp)` - Set depth testing
- `void setBlendState(VkBool32 blendEnable, ...)` - Set alpha blending
- `size_t hash() const` - Generate configuration hash for caching
- `VkGraphicsPipelineCreateInfo getCreateInfo(...) const` - Build pipeline create info

---

## src/vulkan/shadermodule.h

### Class: `ShaderModule`
SPIR-V shader module loading and lifecycle (RAII).

**Public Static Methods:**
- `static ShaderModule fromSpirV(VkDevice device, const std::string& filepath, VkShaderStageFlagBits stage)` - Create from SPIR-V file
- `static std::vector<char> readFile(const std::string& filepath)` - Read binary file

**Public Methods:**
- `const VulkanShaderModuleHandle& getHandle() const` - Get shader module handle
- `VkShaderStageFlagBits getStage() const` - Get shader stage
- `VkPipelineShaderStageCreateInfo getStageCreateInfo() const` - Get stage create info for pipeline

---

## src/vulkan/shaderprogram.h

### Class: `ShaderProgram`
Groups shader stages for graphics pipelines.

**Public Static Methods:**
- `static ShaderProgram createGraphicsProgram(VkDevice device, const std::string& vertPath, const std::string& fragPath)` - Factory for graphics

**Public Methods:**
- `std::vector<VkPipelineShaderStageCreateInfo> getShaderStages() const` - Get all stage create infos
- `std::optional<ShaderModule> getVertexShader() const` - Get vertex shader
- `std::optional<ShaderModule> getFragmentShader() const` - Get fragment shader

---

## src/vulkan/commandbuffermanager.h

### Class: `CommandBufferManager`
Centralizes command pool and buffer management.

**Public Methods:**
- `explicit CommandBufferManager(VkDevice device)` - Constructor
- `~CommandBufferManager()` - Destructor
- `bool initialize()` - Initialize manager
- `void cleanup()` - Clean up all pools and buffers
- `VkCommandPool createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0)` - Create command pool
- `std::vector<VkCommandBuffer> allocateCommandBuffers(VkCommandPool, uint32_t count, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY)` - Allocate buffers
- `VkCommandBuffer beginSingleTimeCommands(VkCommandPool)` - Begin one-time command
- `void endSingleTimeCommands(VkCommandBuffer, VkCommandPool, VkQueue)` - End and submit one-time command
- `void resetCommandPool(VkCommandPool, VkCommandPoolResetFlags flags = 0)` - Reset command pool
- `void freeCommandBuffers(VkCommandPool, const std::vector<VkCommandBuffer>&)` - Free buffers
- `bool isInitialized() const` - Check initialization state

**Private Methods:**
- `void validateCommandPool(VkCommandPool) const` - Validate pool was created by manager

---

## src/vulkan/framebuffermanager.h

### Class: `FramebufferManager`
Manages Vulkan framebuffer creation and lifecycle.

**Public Methods:**
- `explicit FramebufferManager(VkDevice device)` - Constructor
- `~FramebufferManager()` - Destructor
- `bool initialize()` - Initialize manager
- `void cleanup()` - Clean up all framebuffers
- `void createSwapChainFramebuffers(VkRenderPass, const std::vector<VulkanImageViewHandle>& swapChainImageViews, VkImageView depthImageView, uint32_t width, uint32_t height)` - Create framebuffers
- `void recreateSwapChainFramebuffers(VkRenderPass, const std::vector<VulkanImageViewHandle>&, VkImageView, uint32_t width, uint32_t height)` - Recreate on resize
- `VkFramebuffer getFramebuffer(size_t index) const` - Get framebuffer by index
- `size_t getFramebufferCount() const` - Get total framebuffer count
- `bool hasFramebuffers() const` - Check if framebuffers exist

**Private Methods:**
- `void validateFramebufferIndex(size_t index) const` - Validate index before access

---

## src/rendering/renderer.h

### Struct: `Renderer::CameraUBO`
Camera uniform buffer data for GPU.

**Members:**
- `glm::mat4 view` - View matrix
- `glm::mat4 projection` - Projection matrix
- `glm::vec3 cameraPos` - Camera position for lighting
- `float padding` - Alignment padding

### Class: `Renderer`
Main renderer coordinating the rendering pipeline (uses Reverse-Z depth buffering).

**Public Methods:**
- `Renderer()` - Constructor
- `~Renderer()` - Destructor
- `bool initialize(SDL_Window* window)` - Initialize with SDL window
- `void cleanup()` - Clean up all Vulkan resources
- `void drawFrame()` - Draw a single frame
- `void update(float deltaTime)` - Update renderer state
- `bool recreateSwapChain(uint32_t newWidth, uint32_t newHeight)` - Recreate swap chain on resize
- `Camera* getCamera()` - Get camera pointer
- `void handleCameraInput(SDL_Window* window, const SDL_Event& event) const` - Handle camera input
- `scene::Scene* getScene()` - Get scene manager
- `LightManager* getLightManager() const` - Get light manager
- `MaterialManager* getMaterialManager() const` - Get material manager
- `std::shared_ptr<scene::SceneNode> loadModel(const std::string& filePath, std::shared_ptr<scene::SceneNode> parentNode)` - Load model synchronously
- `std::future<std::shared_ptr<scene::SceneNode>> loadModelAsync(const std::string& filePath, std::shared_ptr<scene::SceneNode> parentNode = nullptr)` - Load model asynchronously
- `bool captureScreenshot(const std::string& filename)` - Capture screenshot to PNG

**Private Methods:**
- `void createCommandBuffers()` - Create command buffers
- `void createRenderPass()` - Create render pass
- `void createFramebuffers()` - Create framebuffers
- `void cleanupFramebuffers()` - Clean up framebuffers
- `VulkanShaderModuleHandle createShaderModule(const std::vector<char>& code)` - Create shader module
- `void createGraphicsPipeline()` - Create graphics pipeline
- `void recordCommandBuffers()` - Record command buffers
- `void createCameraUniformBuffer()` - Create camera UBO
- `void updateCameraUniformBuffer() const` - Update camera UBO
- `void createDescriptorPool()` - Create descriptor pool
- `void createDescriptorSets()` - Create descriptor sets
- `void createSyncObjects()` - Create synchronization objects
- `void cleanupSyncObjects()` - Clean up sync objects
- `void initializeDepthBuffer()` - Initialize depth buffer
- `void initializeScene()` - Initialize scene
- `void createLightUniformBuffer()` - Create light UBO
- `void updateLightUniformBuffer() const` - Update light UBO
- `void initializeMaterials()` - Initialize materials
- `void initializeModelManager()` - Initialize model manager
- `void initializeModelLoadingComponents()` - Initialize material mapper, texture loader, pipeline factory

---

## src/rendering/camera.h

### Class: `Camera` (abstract)
Base class for all camera implementations using quaternion-based orientation.

**Public Methods:**
- `virtual ~Camera()` - Virtual destructor
- `virtual glm::mat4 getViewMatrix() const = 0` - Get view matrix (pure virtual)
- `virtual glm::mat4 getProjectionMatrix(float aspectRatio) const = 0` - Get projection matrix (pure virtual)
- `virtual void update(float deltaTime) = 0` - Update camera state (pure virtual)
- `virtual void setPosition(const glm::vec3& newPosition)` - Set position
- `virtual glm::vec3 getPosition() const` - Get position
- `virtual void setOrientation(const glm::quat& newOrientation)` - Set orientation
- `virtual glm::quat getOrientation() const` - Get orientation
- `virtual void setFov(float newFov)` - Set field of view
- `virtual float getFov() const` - Get field of view
- `virtual void setNearPlane(float newNearPlane)` - Set near clipping plane
- `virtual float getNearPlane() const` - Get near plane
- `virtual void setFarPlane(float newFarPlane)` - Set far clipping plane
- `virtual float getFarPlane() const` - Get far plane

**Protected Methods:**
- `glm::vec3 getFront() const` - Get front direction vector
- `glm::vec3 getUp() const` - Get up direction vector
- `glm::vec3 getRight() const` - Get right direction vector

---

## src/rendering/editorcamera.h

### Class: `EditorCamera : Camera`
Free-movement first-person camera.

**Public Methods:**
- `EditorCamera(const glm::vec3& position, float yaw, float pitch)` - Constructor
- `void handleInput(const SDL_Event& event, SDL_Window* window)` - Handle SDL events
- `void update(float deltaTime)` - Update camera (override)
- `glm::mat4 getViewMatrix() const` - Get view matrix (override)
- `glm::mat4 getProjectionMatrix(float aspectRatio) const` - Get projection (override)
- `void setMovementSpeed(float speed)` - Set movement speed
- `void setMouseSensitivity(float sensitivity)` - Set mouse sensitivity

---

## src/rendering/orbitcamera.h

### Class: `OrbitCamera : Camera`
Object-centric orbiting camera.

**Public Methods:**
- `OrbitCamera(const glm::vec3& targetPoint, float distance, float horizontalAngle, float verticalAngle, bool clampVerticalAngle = true)` - Constructor
- `void handleInput(const SDL_Event& event, SDL_Window* window)` - Handle SDL events
- `void update(float deltaTime)` - Update camera (override)
- `glm::mat4 getViewMatrix() const` - Get view matrix (override)
- `glm::mat4 getProjectionMatrix(float aspectRatio) const` - Get projection (override)
- `void setTargetPoint(const glm::vec3& target)` - Set orbit target
- `glm::vec3 getTargetPoint() const` - Get orbit target
- `void setDistance(float distance)` - Set distance from target
- `float getDistance() const` - Get distance
- `void setMouseSensitivity(float sensitivity)` - Set mouse sensitivity
- `void setZoomSensitivity(float sensitivity)` - Set zoom sensitivity

---

## src/rendering/material.h

### Enum: `Material::CullingMode`
Culling mode configuration.

**Values:**
- `None` - No culling
- `Back` - Back-face culling (default)
- `Front` - Front-face culling (for inverted models)

### Enum: `Material::TextureChannel`
Texture channel selection.

**Values:**
- `R`, `G`, `B`, `A` - Individual channels

### Class: `Material` (abstract, non-copyable)
Base class for all materials.

**Public Methods:**
- `virtual ~Material()` - Virtual destructor
- `virtual ShaderPaths getShaderPaths() const = 0` - Get shader paths (pure virtual)
- `virtual PipelineConfig getPipelineConfig() const` - Get pipeline configuration
- `MaterialType getType() const` - Get material type
- `MaterialFeatureFlags getFeatures() const` - Get feature flags
- `const std::string& getName() const` - Get material name
- `bool hasFeature(MaterialFeatureFlags feature) const` - Check feature enabled
- `void setCullingMode(CullingMode cullingMode)` - Set culling mode
- `virtual void bind(VkCommandBuffer, VkPipelineLayout) const` - Bind for rendering
- `virtual VkDescriptorSetLayout getDescriptorSetLayout() const` - Get descriptor layout

**Protected Methods:**
- `Material(VkDevice, const std::string& name, VkPhysicalDevice, MaterialType type, MaterialFeatureFlags features)` - Constructor
- `PipelineConfig getDefaultConfig() const` - Get default pipeline config
- `virtual bool createDescriptorPool()` - Create descriptor pool
- `virtual void configurePipeline(PipelineConfig& config) const` - Customize pipeline

---

## src/rendering/pbrmaterial.h

### Class: `PBRMaterial : Material`
PBR material with metallic-roughness workflow.

**Public Methods:**
- Texture setters:
  - `void setAlbedoTexture(std::shared_ptr<Texture>)` - Set base color texture
  - `void setNormalMap(std::shared_ptr<Texture>)` - Set normal map
  - `void setRoughnessMap(std::shared_ptr<Texture>)` - Set roughness map
  - `void setMetallicMap(std::shared_ptr<Texture>)` - Set metallic map
  - `void setOcclusionMap(std::shared_ptr<Texture>)` - Set AO map
  - `void setRoughnessMetallicMap(std::shared_ptr<Texture>)` - Set combined roughness-metallic
  - `void setOcclusionRoughnessMetallicMap(std::shared_ptr<Texture>)` - Set ORM map
- Parameter setters:
  - `void setBaseColor(const glm::vec4&)` - Set base color
  - `void setRoughness(float)` - Set roughness value
  - `void setMetallic(float)` - Set metallic value
  - `void setAmbient(float)` - Set ambient occlusion value
  - `void setNormalStrength(float)` - Set normal map strength
  - `void setRoughnessStrength(float)` - Set roughness strength
  - `void setMetallicStrength(float)` - Set metallic strength
  - `void setOcclusionStrength(float)` - Set occlusion strength
  - `void setTextureTiling(float u, float v)` - Set UV tiling
- Getters for all properties

---

## src/rendering/debugmaterial.h

### Enum: `DebugMaterial::VisualizationMode`
Debug visualization modes.

**Values:**
- `VertexColors` - Show vertex colors
- `NormalColors` - Show normals as colors
- `WindingOrder` - Visualize triangle winding

### Class: `DebugMaterial : Material`
Debug visualization material.

**Public Methods:**
- `void setVisualizationMode(VisualizationMode mode)` - Set visualization mode
- `VisualizationMode getVisualizationMode() const` - Get current mode
- `void setColorMultiplier(const glm::vec4& color)` - Set color tint
- `glm::vec4 getColorMultiplier() const` - Get color tint

---

## src/rendering/wireframematerial.h

### Class: `WireframeMaterial : Material`
Wireframe rendering material.

**Public Methods:**
- `void setColor(const glm::vec4& color)` - Set wireframe color
- `glm::vec4 getColor() const` - Get wireframe color

---

## src/rendering/terrainmaterial.h

### Struct: `NoiseParameters`
Noise generation parameters.

**Members:**
- `float frequency`, `amplitude`, `octaves`, `persistence`, `lacunarity`

### Struct: `TransitionParameters`
Biome transition configuration.

**Members:**
- `int type`, `float scale`, `float sharpness`, `NoiseParameters noise`

### Struct: `BiomeParameters`
Biome visual and physical properties.

**Members:**
- `glm::vec3 color`, `cliffColor`
- `float minHeight`, `maxHeight`, `minSteepness`, `roughness`, `metallic`

### Enum: `TerrainMaterial::TerrainDebugMode`
Debug visualization modes.

**Values:**
- `None`, `Height`, `Steepness`, `Normals`, `Tangents`, `Bitangents`, `UVCoordinates`

### Class: `TerrainMaterial : Material`
Height-based biome visualization for planetary terrain.

**Public Methods:**
- `void setBiome(int biomeIndex, const BiomeParameters& params)` - Configure biome
- `void setPlanetRadius(float radius)` - Set planet radius
- `BiomeParameters& getProperties(int biomeIndex)` - Get biome properties
- `void setNoiseParameters(int biomeIndex, const NoiseParameters& noise)` - Set biome noise
- `void setDebugMode(TerrainDebugMode mode)` - Set debug visualization
- `TerrainDebugMode getDebugMode() const` - Get debug mode

---

## src/rendering/light.h

### Struct: `LightData` (GPU-aligned)
GPU light data structure.

**Members:**
- `glm::vec4 direction` - Light direction
- `glm::vec4 colorAndIntensity` - RGB + intensity
- `glm::vec4 ambient` - Ambient contribution

### Class: `Light` (abstract)
Base light class.

**Public Methods:**
- `virtual ~Light()` - Virtual destructor
- `virtual LightData getLightData() const = 0` - Get GPU data (pure virtual)
- `virtual void setColor(const glm::vec3& color) = 0` - Set color (pure virtual)
- `virtual glm::vec3 getColor() const = 0` - Get color (pure virtual)
- `virtual void setIntensity(float intensity) = 0` - Set intensity (pure virtual)
- `virtual float getIntensity() const = 0` - Get intensity (pure virtual)
- `virtual void setAmbient(const glm::vec3& ambient) = 0` - Set ambient (pure virtual)
- `virtual glm::vec3 getAmbient() const = 0` - Get ambient (pure virtual)

### Class: `DirectionalLight : Light`
Directional light implementation.

**Public Methods:**
- `DirectionalLight(const glm::vec3& direction, const glm::vec3& color, float intensity)` - Constructor
- `void setDirection(const glm::vec3& direction)` - Set normalized direction
- `glm::vec3 getDirection() const` - Get direction
- All inherited virtual methods (override)

---

## src/rendering/lightmanager.h

### Class: `LightManager`
Centralized light management.

**Public Constants:**
- `static constexpr size_t MaxLights = 16` - Maximum light count

**Public Methods:**
- `size_t addLight(std::shared_ptr<Light> light)` - Add light, return index
- `void removeLight(size_t index)` - Remove light by index
- `void removeAllLights()` - Clear all lights
- `std::shared_ptr<Light> getLight(size_t index) const` - Get light by index
- `const std::vector<std::shared_ptr<Light>>& getLights() const` - Get all lights
- `size_t getLightCount() const` - Get light count
- `std::vector<LightData> getLightData() const` - Prepare GPU-format data
- `bool canAddLight() const` - Check capacity

---

## src/rendering/materialmanager.h

### Class: `MaterialManager`
Centralized material creation and caching.

**Public Methods:**
- `std::shared_ptr<PBRMaterial> createPBRMaterial(const std::string& name)` - Create or get PBR material
- `std::shared_ptr<CustomMaterial> createCustomMaterial(const std::string& name)` - Create custom material
- `std::shared_ptr<WireframeMaterial> createWireframeMaterial(const std::string& name)` - Create wireframe
- `std::shared_ptr<TerrainMaterial> createTerrainMaterial(const std::string& name)` - Create terrain
- `std::shared_ptr<DebugMaterial> createDebugMaterial(const std::string& name)` - Create debug
- `std::shared_ptr<Material> getMaterial(const std::string& name) const` - Get by name
- `bool hasMaterial(const std::string& name) const` - Check existence
- `const std::unordered_map<std::string, std::shared_ptr<Material>>& getMaterials() const` - Get all
- `void cleanup()` - Clean up resources

---

## src/rendering/mesh.h

### Struct: `Mesh::RenderData`
Everything needed to render a mesh.

**Members:**
- `glm::mat4 modelMatrix` - Transform matrix
- `std::shared_ptr<VertexBuffer> vertexBuffer` - Vertex buffer
- `std::shared_ptr<IndexBuffer> indexBuffer` - Index buffer
- `std::shared_ptr<Material> material` - Material

### Class: `Mesh` (abstract)
Base mesh class with CPU and GPU data.

**Public Methods:**
- `virtual ~Mesh()` - Virtual destructor
- `virtual void generateGeometry() = 0` - Generate mesh geometry (pure virtual)
- `virtual void prepareRenderData(RenderData& data) const` - Populate render data
- `const std::vector<Vertex>& getVertices() const` - Get vertex data
- `const std::vector<uint32_t>& getIndices() const` - Get index data
- `void setBuffers(std::shared_ptr<VertexBuffer>, std::shared_ptr<IndexBuffer>)` - Set GPU buffers
- `void setTranslation(const glm::vec3&)` - Set translation
- `void setMaterial(std::shared_ptr<Material>)` - Set material
- `std::shared_ptr<Material> getMaterial() const` - Get material
- `void markBuffersDirty()` - Mark buffers needing update
- `bool needsBufferUpdate() const` - Check if update needed
- `void clearBuffersDirty()` - Clear dirty flag
- `void setTextureTiling(float u, float v)` - Set texture tiling
- `float getTextureTilingU() const` - Get U tiling
- `float getTextureTilingV() const` - Get V tiling

**Protected Methods:**
- `glm::vec2 applyTextureTiling(const glm::vec2& uv) const` - Apply tiling to UV

---

## src/rendering/cubemesh.h

### Class: `CubeMesh : Mesh`
Procedural cube mesh.

**Public Methods:**
- `explicit CubeMesh(float sideLength = 1.0f)` - Constructor
- `void generateGeometry()` - Generate cube (override)
- `void setFaceColors(const std::array<glm::vec3, 6>& colors)` - Set per-face colors

---

## src/rendering/icospheremesh.h

### Struct: `IcosphereMesh::VertexTransform`
Vertex transformation data.

**Members:**
- `glm::vec3 newPosition`, `newNormal`, `newColor`

### Class: `IcosphereMesh : Mesh`
Sphere via icosahedron subdivision.

**Public Methods:**
- `IcosphereMesh(float radius = 1.0f, int subdivisions = 0)` - Constructor
- `void generateGeometry()` - Generate sphere (override)
- `void applyVertexTransforms(const std::vector<VertexTransform>& transforms)` - Batch vertex updates
- `std::vector<glm::vec3> getVertexPositions() const` - Get positions
- `float getRadius() const` - Get radius
- `int getSubdivisions() const` - Get subdivision level

**Private Methods:**
- `void initializeBaseIcosahedron()` - Create base icosahedron
- `void subdivide()` - Subdivide once
- `uint32_t getOrCreateMidpoint(uint32_t v1, uint32_t v2, std::unordered_map<uint64_t, uint32_t>& midpointCache)` - Get/create midpoint
- `static uint64_t generateEdgeKey(uint32_t v1, uint32_t v2)` - Edge hash

---

## src/rendering/meshmanager.h

### Class: `MeshManager`
Mesh creation and buffer management.

**Public Methods:**
- `template<typename MeshType, typename... Args> std::shared_ptr<MeshType> createMesh(Args&&... args)` - Create procedural mesh
- `template<typename MeshType> std::shared_ptr<MeshType> createMeshWithGeometry(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)` - Create with data
- `void updateBuffers(Mesh* mesh)` - Update GPU buffers
- `void updateBuffersIfNeeded(Mesh* mesh)` - Conditional buffer update

---

## src/rendering/texture.h

### Enum: `Texture::FilterMode`
Texture filtering modes.

**Values:**
- `Nearest`, `Linear`, `Cubic`

### Enum: `Texture::WrapMode`
Texture wrapping modes.

**Values:**
- `Repeat`, `MirroredRepeat`, `ClampToEdge`, `ClampToBorder`

### Class: `Texture`
GPU texture resource with image, view, and sampler.

**Public Methods:**
- `Texture(VkDevice, VkPhysicalDevice, uint32_t width, uint32_t height, VkFormat format, uint32_t mipLevels, uint32_t layerCount, const std::string& name)` - Constructor
- `~Texture()` - Destructor
- `void uploadData(const void* data, size_t size, VkCommandPool, VkQueue, CommandBufferManager&)` - Upload pixel data
- `void configureSampler(FilterMode min, FilterMode mag, WrapMode wrapU, WrapMode wrapV, bool anisotropy, float maxAnisotropy)` - Configure sampler
- `void generateMipmaps(VkCommandPool, VkQueue, CommandBufferManager&)` - Generate mipmaps
- `VkImageView getImageView() const` - Get image view
- `VkSampler getSampler() const` - Get sampler
- `uint32_t getWidth() const` - Get width
- `uint32_t getHeight() const` - Get height
- `VkFormat getFormat() const` - Get format
- `uint32_t getMipLevels() const` - Get mipmap levels
- `bool hasAlpha() const` - Check alpha channel
- `const std::string& getName() const` - Get name

**Private Methods:**
- `void transitionLayout(CommandBufferManager&, VkCommandPool, VkQueue, VkImageLayout old, VkImageLayout new, uint32_t baseMip, uint32_t levelCount, uint32_t baseLayer, uint32_t layerCount)` - Transition image layout
- `static uint32_t calculateMipLevels(uint32_t width, uint32_t height)` - Calculate mip levels
- `uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const` - Find memory type
- `bool isFormatSupported(VkFormat, VkFormatFeatureFlags) const` - Check format support
- `static VkFilter toVkFilter(FilterMode)` - Convert filter mode
- `static VkSamplerAddressMode toVkAddressMode(WrapMode)` - Convert wrap mode

---

## src/rendering/texturemanager.h

### Class: `TextureManager`
Texture loading, caching, and lifecycle.

**Public Methods:**
- `std::shared_ptr<Texture> getOrLoadTexture(const std::string& name, const std::string& filePath)` - Load with caching
- `std::shared_ptr<Texture> createTexture(const std::string& name, uint32_t width, uint32_t height, const void* data, size_t dataSize, VkFormat format)` - Create from data
- `std::shared_ptr<Texture> createTextureFromBufferView(const std::string& name, const std::vector<unsigned char>& data, int width, int height, int channels)` - Create from buffer view
- `bool isTextureLoaded(const std::string& name) const` - Check cache
- `std::shared_ptr<Texture> getTexture(const std::string& name) const` - Get cached texture
- `void releaseTexture(const std::string& name)` - Release texture
- `void releaseAllTextures()` - Clear cache
- `std::shared_ptr<Texture> createDefaultTexture()` - Create white default
- `std::shared_ptr<Texture> getDefaultTexture() const` - Get default texture

---

## src/rendering/textureloader.h

### Struct: `TextureLoader::TextureData`
Loaded texture data.

**Members:**
- `unsigned char* pixels`
- `int width`, `height`, `channels`
- `bool success`
- `std::string errorMessage`

### Enum: `TextureLoader::Format`
Target pixel formats.

**Values:**
- `Keep`, `RGB`, `RGBA`, `R`, `NormalMap`

### Class: `TextureLoader`
Image loading from files and memory.

**Public Static Methods:**
- `static TextureData loadFromFile(const std::string& filepath, Format desiredFormat)` - Load from file
- `static TextureData loadFromMemory(const unsigned char* buffer, size_t size, Format desiredFormat)` - Load from buffer
- `static TextureData loadFromBufferView(const std::vector<unsigned char>& buffer, Format desiredFormat)` - Load from buffer view

---

## src/rendering/buffermanager.h

### Class: `BufferManager`
Centralized GPU buffer creation.

**Public Methods:**
- `bool initialize()` - Initialize command pool
- `void cleanup()` - Clean up resources
- `std::shared_ptr<VertexBuffer> createVertexBuffer(const std::vector<Vertex>& vertices)` - Create vertex buffer
- `std::shared_ptr<IndexBuffer> createIndexBuffer(const std::vector<uint32_t>& indices)` - Create index buffer
- `std::shared_ptr<Buffer> createUniformBuffer(VkDeviceSize size)` - Create uniform buffer
- `std::shared_ptr<Buffer> createStorageBuffer(VkDeviceSize size, const void* data)` - Create storage buffer
- `std::shared_ptr<Buffer> createStagingBuffer(VkDeviceSize size, const void* data)` - Create staging buffer
- `void updateBuffer(std::shared_ptr<Buffer> buffer, const void* data, VkDeviceSize size, VkDeviceSize offset)` - Update buffer
- `void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size)` - Copy buffer
- `BufferCache* getBufferCache()` - Get buffer cache

---

## src/rendering/buffercache.h

### Class: `BufferCache`
GPU buffer reuse via bucketing.

**Public Methods:**
- `std::shared_ptr<VertexBuffer> getOrCreateVertexBuffer(const std::vector<Vertex>& vertices, BufferManager* bufferManager)` - Get or create vertex buffer
- `std::shared_ptr<IndexBuffer> getOrCreateIndexBuffer(const std::vector<uint32_t>& indices, BufferManager* bufferManager)` - Get or create index buffer
- `void cleanup()` - Clear cache
- `bool hasActiveBuffers() const` - Check for in-use buffers

**Private Methods:**
- `static size_t calculateBufferBucket(VkDeviceSize size)` - Calculate size bucket

---

## src/rendering/pipelinefactory.h

### Class: `PipelineFactory`
Pipeline creation for model materials.

**Public Methods:**
- `void createPipelinesForModel(const std::vector<std::shared_ptr<Material>>& materials)` - Create pipelines for all materials
- `void createPipelineForMaterial(const std::shared_ptr<Material>& material)` - Create single pipeline
- `bool hasPipeline(const std::string& materialName) const` - Check existence
- `void clearCache()` - Clear pipeline cache

**Private Methods:**
- `void configureMaterialFeatures(const ModelData::MaterialInfo& materialInfo, PBRMaterial* material)` - Extract features
- `void setStandardMaterialParams(const ModelData::MaterialInfo& materialInfo, PBRMaterial* material)` - Map material data

---

## src/rendering/screenshot.h

### Class: `Screenshot`
Screenshot capture to PNG.

**Public Methods:**
- `bool captureScreenshot(VkDevice, VkPhysicalDevice, VkImage, VkFormat, uint32_t width, uint32_t height, VkQueue, VkCommandPool, CommandBufferManager*, const std::string& filename)` - Capture to PNG

**Private Methods:**
- `void transitionImageLayout(VkCommandBuffer, VkImage, VkFormat, VkImageLayout old, VkImageLayout new)` - Transition layout
- `VulkanBufferHandle createScreenshotBuffer(VkDevice, VkPhysicalDevice, VkDeviceSize size, VkDeviceMemory& memory)` - Create staging buffer
- `bool saveScreenshotToPNG(const unsigned char* data, uint32_t width, uint32_t height, const std::string& filename)` - Write PNG
- `void cleanup()` - Clean up resources
- `uint32_t getFormatSize(VkFormat format)` - Get pixel size
- `std::vector<unsigned char> convertToRGBA(const unsigned char* data, uint32_t width, uint32_t height, VkFormat format)` - Format conversion

---

## src/rendering/vertex.h

### Struct: `Vertex`
Vertex structure with geometry and attributes.

**Members:**
- `glm::vec3 position` - Vertex position
- `glm::vec3 normal` - Vertex normal
- `glm::vec3 tangent` - Tangent vector for normal mapping
- `glm::vec3 color` - Vertex color
- `glm::vec2 texCoord` - Texture coordinates

**Static Methods:**
- `static VkVertexInputBindingDescription getBindingDescription()` - Get binding description
- `static std::array<VkVertexInputAttributeDescription, 5> getAttributeDescriptions()` - Get attribute descriptions
- `bool operator==(const Vertex& other) const` - Equality comparison

---

## src/rendering/tangentcalculator.h

### Class: `TangentCalculator` (static only)
Tangent space calculation.

**Public Static Methods:**
- `static void calculateTangents(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)` - Calculate for triangles
- `static void calculateTangentsForQuads(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)` - Calculate for quads

---

## src/scene/scenetypes.h

### Type: `NodeID`
64-bit unique scene node identifier.

### Enum: `BoundsType`
Bounding volume strategies.

**Values:**
- `Box`, `Sphere`, `OBB`

### Enum: `VisibilityStatus`
Culling/visibility state.

**Values:**
- `Visible`, `Culled`, `PartiallyVisible`

### Struct: `Transform`
Position, rotation, scale with matrix conversion.

**Members:**
- `glm::vec3 position`, `scale`
- `glm::quat rotation`

**Methods:**
- `glm::mat4 toMatrix() const` - Convert to matrix
- `static Transform fromMatrix(const glm::mat4&)` - Create from matrix

---

## src/scene/scenenode.h

### Class: `SceneNode`
Hierarchical scene graph node (shared_ptr, non-copyable).

**Public Methods:**
- `void addChild(std::shared_ptr<SceneNode> child)` - Add child node
- `bool removeChild(const std::shared_ptr<SceneNode>& child)` - Remove child
- `void setMesh(std::shared_ptr<rendering::Mesh> mesh)` - Associate mesh
- `void setLocalTransform(const Transform& transform)` - Set local transform
- `Transform getWorldTransform() const` - Get combined transform
- `Transform getLocalTransform() const` - Get local transform
- `std::weak_ptr<SceneNode> getParent() const` - Get parent
- `const std::vector<std::shared_ptr<SceneNode>>& getChildren() const` - Get children
- `std::shared_ptr<rendering::Mesh> getMesh() const` - Get mesh
- `scene::BoundingBox getWorldBounds() const` - Get world-space bounds
- `void updateWorldTransform()` - Propagate transform to hierarchy
- `bool isVisible(const Frustum& frustum) const` - Frustum culling test
- `void getRenderData(const Frustum& frustum, std::vector<rendering::Mesh::RenderData>& outRenderData) const` - Collect visible render data

---

## src/scene/frustum.h

### Struct: `Frustum::Plane`
Frustum plane with normal and distance.

**Members:**
- `glm::vec3 normal`
- `float distance`

**Methods:**
- `bool isInFront(const glm::vec3& point) const` - Point-plane test

### Class: `Frustum`
View frustum for culling.

**Public Static Methods:**
- `static Frustum createFromMatrix(const glm::mat4& viewProjection)` - Extract from VP matrix

**Public Methods:**
- `bool containsPoint(const glm::vec3& point) const` - Point-in-frustum test
- `bool intersectsBox(const BoundingBox& box) const` - Box intersection (primary culling)
- `std::array<glm::vec3, 8> getCorners() const` - Get frustum corners

---

## src/scene/boundingbox.h

### Class: `BoundingBox`
Axis-aligned bounding box.

**Public Methods:**
- `BoundingBox()` - Default constructor (invalid state)
- `void reset()` - Clear to invalid state
- `void addPoint(const glm::vec3& point)` - Expand to contain point
- `BoundingBox transform(const glm::mat4& matrix) const` - Transform box
- `bool intersects(const BoundingBox& other) const` - Box-box collision
- `bool contains(const glm::vec3& point) const` - Point-in-box test
- `std::array<glm::vec3, 8> getCorners() const` - Get 8 corners
- `glm::vec3 getCenter() const` - Get center point
- `glm::vec3 getSize() const` - Get dimensions
- `bool isValid() const` - Check validity
- `glm::vec3 getMin() const` - Get minimum corner
- `glm::vec3 getMax() const` - Get maximum corner

---

## src/scene/scene.h

### Class: `Scene` (non-copyable)
Top-level scene manager.

**Public Methods:**
- `std::shared_ptr<SceneNode> createNode(std::shared_ptr<SceneNode> parent = nullptr)` - Create node
- `bool removeNode(const std::shared_ptr<SceneNode>& node)` - Remove node and children
- `void update(float deltaTime)` - Update transforms and bounds
- `std::vector<rendering::Mesh::RenderData> getRenderData(const Frustum& frustum) const` - Get visible objects
- `std::shared_ptr<SceneNode> getRoot() const` - Get root node
- `size_t getNodeCount() const` - Get total node count
- `void setTerrainRoot(std::shared_ptr<SceneNode> terrainRoot)` - Set terrain node
- `std::shared_ptr<SceneNode> getTerrainRoot() const` - Get terrain node
- `void forEachMesh(const std::function<void(rendering::Mesh*)>& func)` - Apply function to all meshes

---

## src/rendering/models/modeldata.h

### Struct: `ModelMeshData`
Intermediate mesh data.

**Members:**
- `std::vector<Vertex> vertices`
- `std::vector<uint32_t> indices`
- `std::string materialName`

### Struct: `ModelData::MaterialInfo`
PBR material properties.

**Members:**
- `glm::vec4 baseColor`
- `float roughness`, `metallic`
- `std::string albedoTexture`, `normalTexture`, `roughnessTexture`, `metallicTexture`, `occlusionTexture`
- `bool hasTransparency`
- `float alphaMode`

### Struct: `ModelData::NodeInfo`
Hierarchy node data.

**Members:**
- `glm::mat4 transform`
- `std::vector<size_t> childIndices`
- `int meshIndex`

### Struct: `ModelData`
Complete model representation.

**Members:**
- `std::vector<ModelMeshData> meshes`
- `std::unordered_map<std::string, MaterialInfo> materials`
- `std::vector<NodeInfo> nodes`
- `size_t rootNodeIndex`

---

## src/rendering/models/modelloader.h

### Struct: `ModelLoadOptions`
Model loading configuration.

**Members:**
- `bool calculateTangents`
- `bool generateMipmaps`
- `bool loadAnimations`
- `float scale`

### Class: `ModelLoader` (abstract)
Abstract model format interface.

**Public Methods:**
- `virtual std::shared_ptr<scene::SceneNode> loadModel(const std::string& filePath, scene::Scene* scene, std::shared_ptr<scene::SceneNode> parentNode, const ModelLoadOptions& options) = 0` - Load model (pure virtual)
- `virtual bool supportsFormat(const std::string& extension) const = 0` - Format detection (pure virtual)

---

## src/rendering/models/modelmanager.h

### Class: `ModelManager`
Centralized model loading with caching.

**Public Methods:**
- `void registerLoader(std::unique_ptr<ModelLoader> loader)` - Register format loader
- `void initialize()` - Register default loaders
- `std::shared_ptr<scene::SceneNode> loadModel(const std::string& filePath, std::shared_ptr<scene::SceneNode> parentNode, const ModelLoadOptions& options)` - Load synchronously
- `std::future<std::shared_ptr<scene::SceneNode>> loadModelAsync(const std::string& filePath, std::shared_ptr<scene::SceneNode> parentNode, const ModelLoadOptions& options)` - Load asynchronously
- `bool isLoadingAsync() const` - Check async status
- `void waitForAsyncOperations()` - Wait for all loads
- `bool isModelLoaded(const std::string& filePath) const` - Check cache
- `std::shared_ptr<scene::SceneNode> instantiateModel(const std::string& filePath, std::shared_ptr<scene::SceneNode> parentNode)` - Instantiate from cache
- `void unloadModel(const std::string& filePath)` - Remove from cache
- `void clearCache()` - Clear all cached models
- `void setResourceBaseDirectory(const std::string& dir)` - Set resource path

---

## src/rendering/models/gltfmodelloader.h

### Class: `GltfModelLoader : ModelLoader`
glTF/GLB format loader.

**Public Methods:**
- `std::shared_ptr<scene::SceneNode> loadModel(...) override` - Load glTF (override)
- `bool supportsFormat(const std::string& extension) const override` - Check .gltf/.glb (override)

**Private Methods:**
- `ModelData parseGltfModel(const tinygltf::Model& gltfModel, const ModelLoadOptions& options)` - Convert to ModelData
- `ModelMeshData extractMeshData(const tinygltf::Model&, const tinygltf::Primitive&, const glm::mat4& transform)` - Extract mesh
- `ModelData::MaterialInfo extractMaterialInfo(const tinygltf::Model&, int materialIndex)` - Extract material
- `std::vector<std::shared_ptr<Material>> createMaterials(const ModelData&)` - Build materials
- `std::vector<std::shared_ptr<Mesh>> createMeshes(const ModelData&, const std::vector<std::shared_ptr<Material>>&)` - Build meshes
- `template<typename T> std::vector<T> getAccessorData(const tinygltf::Model&, int accessorIndex)` - Get buffer data
- `std::string getTexturePath(const tinygltf::Model&, int textureIndex)` - Resolve texture
- `void normalizeModelTransform(ModelData& modelData)` - Fit to viewport
- `BoundingBox collectNodeBounds(const ModelData&, size_t nodeIndex, const glm::mat4& parentTransform)` - Accumulate bounds

---

## src/rendering/models/meshextractor.h

### Class: `MeshExtractor`
Extract mesh geometry from glTF primitives.

**Public Methods:**
- `static ModelMeshData extractMeshData(const tinygltf::Model&, const tinygltf::Primitive&, const glm::mat4& transform)` - Main extraction

**Private Static Methods:**
- `static std::vector<glm::vec3> extractPositions(const tinygltf::Model&, const tinygltf::Primitive&, const glm::mat4&)` - Extract positions
- `static std::vector<glm::vec3> extractNormals(const tinygltf::Model&, const tinygltf::Primitive&, const glm::mat4&)` - Extract normals
- `static std::vector<glm::vec2> extractTextureCoords(const tinygltf::Model&, const tinygltf::Primitive&)` - Extract UVs
- `static std::vector<glm::vec3> extractColors(const tinygltf::Model&, const tinygltf::Primitive&)` - Extract colors
- `static std::vector<glm::vec3> extractTangents(const tinygltf::Model&, const tinygltf::Primitive&)` - Extract tangents
- `static std::vector<uint32_t> extractIndices(const tinygltf::Model&, const tinygltf::Primitive&)` - Extract indices
- `static template<typename T> std::vector<T> getAccessorData(const tinygltf::Model&, int accessorIndex)` - Raw buffer access
- `static size_t getComponentSize(int componentType)` - Type size
- `static size_t getComponentCount(int type)` - Type count
- `static bool validatePrimitiveTopology(const tinygltf::Primitive&)` - Topology check

---

## src/rendering/models/materialextractor.h

### Class: `MaterialExtractor`
Extract material properties from glTF.

**Public Methods:**
- `void setEmbeddedTextureExtractor(EmbeddedTextureExtractor* extractor)` - Set texture extractor
- `ModelData::MaterialInfo extractMaterialInfo(const tinygltf::Model&, const tinygltf::Material&)` - Extract material
- `std::string getMaterialName(const tinygltf::Material&, int index)` - Generate name
- `std::unordered_map<std::string, ModelData::MaterialInfo> extractAllMaterials(const tinygltf::Model&)` - Batch extraction

**Private Methods:**
- `std::string getTexturePath(const tinygltf::Model&, int textureIndex)` - Resolve texture
- `void extractEmissiveProperties(const tinygltf::Material&, ModelData::MaterialInfo&)` - Extract emissive
- `void extractTransparencyProperties(const tinygltf::Material&, ModelData::MaterialInfo&)` - Extract transparency

---

## src/rendering/models/embeddedtextureextractor.h

### Class: `EmbeddedTextureExtractor`
Extract embedded glTF textures.

**Public Methods:**
- `void extractTextures(const tinygltf::Model& model)` - Extract all images
- `std::string getTextureName(int textureIndex) const` - Get mapped name
- `bool hasTexture(int textureIndex) const` - Check extraction

**Private Methods:**
- `void extractImage(const tinygltf::Image& image, int imageIndex)` - Extract single image
- `std::string generateTextureName(int imageIndex, const std::string& name)` - Generate name
- `VkFormat determineTextureFormat(const std::string& mimeType, int channels)` - MIME to format

---

## src/rendering/models/materialparametermapper.h

### Class: `MaterialParameterMapper`
Map model material to engine PBR material.

**Public Methods:**
- `void applyParameters(const ModelData::MaterialInfo& materialInfo, PBRMaterial* material)` - Apply all properties

**Private Methods:**
- `void applyScalarParameters(const ModelData::MaterialInfo&, PBRMaterial*)` - Apply scalars
- `void applyTextures(const ModelData::MaterialInfo&, PBRMaterial*)` - Load textures
- `void applyAlbedoTexture(const std::string& path, PBRMaterial*)` - Apply albedo
- `void applyNormalTexture(const std::string& path, PBRMaterial*)` - Apply normal
- `void applyRoughnessTexture(const std::string& path, PBRMaterial*)` - Apply roughness
- `void applyMetallicTexture(const std::string& path, PBRMaterial*)` - Apply metallic
- `void applyOcclusionTexture(const std::string& path, PBRMaterial*)` - Apply occlusion
- `bool isSameTexture(const std::string& path1, const std::string& path2)` - Detect packed textures
- `std::shared_ptr<Texture> resolveTexture(const std::string& path)` - Unified loading
- `bool isEmbeddedTexture(const std::string& path)` - Check embedded
- `std::string resolveTexturePath(const std::string& path)` - Path resolution

---

## src/rendering/models/scenegraphconstructor.h

### Class: `SceneGraphConstructor`
Build scene hierarchy from glTF nodes.

**Public Methods:**
- `static std::shared_ptr<scene::SceneNode> buildSceneGraph(const ModelData& modelData, const std::vector<std::shared_ptr<Mesh>>& meshes, scene::Scene* scene, std::shared_ptr<scene::SceneNode> parentNode)` - Build scene graph

**Private Static Methods:**
- `static std::shared_ptr<scene::SceneNode> processNode(const ModelData&, size_t nodeIndex, const std::vector<std::shared_ptr<Mesh>>&, scene::Scene*, std::shared_ptr<scene::SceneNode> parent)` - Process node recursively
- `static void applyNodeTransform(std::shared_ptr<scene::SceneNode> node, const glm::mat4& transform)` - Apply transform
- `static void assignNodeMesh(std::shared_ptr<scene::SceneNode> node, int meshIndex, const std::vector<std::shared_ptr<Mesh>>&)` - Assign mesh
- `static void handlePrimitiveGroups(...)` - Handle multi-primitive meshes

---

## src/rendering/models/textureloadingpipeline.h

### Struct: `TextureLoadOptions`
Texture loading configuration.

**Members:**
- `bool generateMipmaps`
- `bool enableAnisotropicFiltering`
- `bool convertToSRGB`
- `bool enableCaching`

### Class: `TextureLoadingPipeline`
Async texture loading with caching.

**Public Methods:**
- `void setBaseDirectory(const std::string& dir)` - Set base path
- `std::string getBaseDirectory() const` - Get base path
- `std::future<std::shared_ptr<Texture>> requestTextureAsync(const std::string& filePath, const TextureLoadOptions& options)` - Queue async load
- `std::shared_ptr<Texture> loadTexture(const std::string& filePath, const TextureLoadOptions& options)` - Synchronous load
- `bool isTextureLoading(const std::string& filePath) const` - Check async status
- `void waitForAll()` - Wait for all loads
- `void processCompletedOperations()` - Clean up finished tasks
- `void setDefaultOptions(const TextureLoadOptions& options)` - Set global defaults
- `TextureLoadOptions getDefaultOptions() const` - Get defaults
- `size_t getPendingOperationCount() const` - Get pending count

---

## modules/planet/src/planet/planetgenerator.h

### Struct: `PlanetGenerator::GeneratorSettings`
Terrain noise parameters.

**Members:**
- `float baseFrequency`, `amplitude`
- `int octaves`
- `float persistence`, `lacunarity`
- `int seed`

### Class: `PlanetGenerator`
Terrain generation and modification.

**Public Methods:**
- `PlanetGenerator(std::shared_ptr<PlanetData> planetData, std::shared_ptr<rendering::IcosphereMesh> mesh)` - Constructor
- `void generateTerrain()` - Generate and update mesh
- `void modifyTerrain(const glm::vec3& point, float radius, float strength)` - Modify elevation
- `void setSettings(const GeneratorSettings& settings)` - Set parameters
- `GeneratorSettings getSettings() const` - Get parameters

**Private Methods:**
- `void updateMesh()` - Sync mesh with planet data

---

## modules/planet/src/planet/planetdata.h

### Class: `PlanetData`
Planet mesh data with visitor pattern.

**Public Methods:**
- `void subdivide(int levels)` - Recursive subdivision
- `std::vector<glm::vec3> getVertices() const` - Export vertices
- `std::vector<uint32_t> getIndices() const` - Export indices
- `void applyVisitorToFace(const glm::vec3& point, FaceVisitor& visitor, float radius)` - Apply to face containing point
- `void applyFaceVisitor(FaceVisitor& visitor)` - Apply to all faces
- `void applyVertexVisitor(VertexVisitor& visitor)` - Apply to all vertices
- `std::shared_ptr<Face> getFaceAtPoint(const glm::vec3& point)` - Find face
- `float getHeightAt(const glm::vec3& point)` - Get height at point
- `float getHeightAtNearestVertex(const glm::vec3& point)` - Nearest vertex height
- `float getInterpolatedHeightAt(const glm::vec3& point)` - Smooth height
- `glm::vec3 getNormalAt(const glm::vec3& point)` - Get normal
- `glm::vec3 getNormalAtNearestVertex(const glm::vec3& point)` - Nearest normal
- `glm::vec3 getInterpolatedNormalAt(const glm::vec3& point)` - Smooth normal
- `void updateNormals()` - Recalculate all normals
- `void updateNormalsForVertex(size_t vertexIndex)` - Update single vertex normal
- `void verifyNormalDirections()` - Debug verification

---

## modules/planet/src/planet/face.h

### Struct: `FaceVisitor` (abstract)
Visitor for face operations.

**Public Methods:**
- `virtual void visit(Face& face) = 0` - Visit face (pure virtual)

### Class: `Face`
Triangle face with subdivision hierarchy.

**Public Methods:**
- `Face(size_t v0, size_t v1, size_t v2)` - Constructor
- `void setData(void* data)` - Set generic data
- `void* getData() const` - Get generic data
- `void setNeighbor(int edge, std::shared_ptr<Face> neighbor)` - Set neighbor
- `std::shared_ptr<Face> getNeighbor(int edge) const` - Get neighbor
- `void addChild(std::shared_ptr<Face> child)` - Add subdivision child
- `void setChild(int index, std::shared_ptr<Face> child)` - Set child by index
- `std::shared_ptr<Face> getChild(int index) const` - Get child
- `std::array<std::shared_ptr<Face>, 4> getChildren() const` - Get all 4 children
- `void setParent(std::weak_ptr<Face> parent)` - Set parent
- `std::weak_ptr<Face> getParent() const` - Get parent
- `void setVertexIndices(size_t v0, size_t v1, size_t v2)` - Set vertices
- `std::array<size_t, 3> getVertexIndices() const` - Get vertices
- `glm::vec3 getMidpoint(const std::vector<VertexData>& vertices) const` - Get center
- `glm::vec3 calculateMidpoint(const std::vector<VertexData>& vertices) const` - Calculate center
- `glm::vec3 calculateNormal(const std::vector<VertexData>& vertices) const` - Calculate normal
- `glm::vec3 getNormal() const` - Get normal
- `bool isLeaf() const` - Check if leaf

---

## modules/planet/src/planet/vertexdata.h

### Class: `VertexData` (non-copyable, movable)
Vertex with elevation and slope caching.

**Public Methods:**
- `VertexData(const glm::vec3& position, size_t index)` - Constructor
- `size_t getIndex() const` - Get vertex index
- `float getElevation() const` - Get elevation with dirty tracking
- `void setElevation(float elevation)` - Set elevation
- `glm::vec3 getPosition() const` - Get position
- `glm::vec3 getNormal() const` - Get normal with recalculation
- `void setNormal(const glm::vec3& normal)` - Set normal
- `glm::vec3 getNormal() const` - Get normal
- `void addNeighbor(size_t neighborIndex)` - Add neighbor
- `const std::vector<size_t>& getNeighbors() const` - Get neighbors
- `float getSlope() const` - Get cached slope
- `void calculateNormalFromFaces(const std::vector<glm::vec3>& faceNormals)` - Average normals
- `void clearNeighbors()` - Reset neighbors

**Private Methods:**
- `float calculateSlope() const` - Calculate slope with caching
- `void markSlopesDirty()` - Invalidate slope cache

---

## modules/planet/src/planet/vertexvisitor.h

### Class: `VertexVisitor` (abstract)
Visitor for vertex operations.

**Public Methods:**
- `virtual void visit(VertexData& vertex) = 0` - Visit vertex (pure virtual)

---

## modules/planet/src/planet/noiseterrainvisitor.h

### Class: `NoiseTerrainVisitor : VertexVisitor`
Apply noise-based terrain modifications.

**Public Methods:**
- `NoiseTerrainVisitor(float scale, float magnitude)` - Constructor
- `void visit(VertexData& vertex) override` - Apply noise to vertex

---

## shaders/pbr.glsl.vert

**Description:** PBR vertex shader with Reverse-Z depth and tangent space calculation.

**Inputs:**
- Vertex attributes: position, normal, tangent, color, texCoord

**Outputs:**
- Transformed position (Reverse-Z)
- World position, normal, tangent, bitangent
- Texture coordinates

**Uniform Blocks:**
- CameraUBO (set=0, binding=0): view, projection, cameraPos
- MaterialUBO (set=2, binding=0): PBR properties including tiling

---

## shaders/pbr.glsl.frag

**Description:** PBR fragment shader with metallic-roughness workflow and normal mapping.

**Features:**
- Cook-Torrance BRDF
- Normal mapping with tangent space
- Metallic-roughness workflow
- Combined texture support (roughness-metallic, ORM)
- Multiple lights support (up to 16)

**Inputs:**
- Fragment position, normal, tangent, bitangent
- Texture coordinates

**Outputs:**
- Final color with PBR lighting

**Uniform Blocks:**
- CameraUBO (set=0): view, projection, cameraPos
- LightUBO (set=1): light array, light count
- MaterialUBO (set=2): PBR properties

**Samplers:**
- albedoTexture, normalTexture, roughnessTexture, metallicTexture, occlusionTexture
- roughnessMetallicTexture, occlusionRoughnessMetallicTexture

---

## shaders/debug.glsl.vert

**Description:** Debug visualization vertex shader.

**Inputs:**
- Vertex attributes: position, normal, color

**Outputs:**
- Transformed position
- World normal
- Vertex color

---

## shaders/debug.glsl.frag

**Description:** Debug fragment shader for normals, colors, and winding order visualization.

**Features:**
- Visualization modes: vertex colors, normal colors, winding order
- Color multiplier for tinting

**Inputs:**
- World normal
- Vertex color

**Outputs:**
- Visualization color

---

## shaders/wireframe.glsl.vert

**Description:** Wireframe rendering vertex shader.

**Inputs:**
- Vertex position

**Outputs:**
- Transformed position

---

## shaders/wireframe.glsl.frag

**Description:** Wireframe rendering fragment shader.

**Outputs:**
- Solid wireframe color

---

## shaders/terrain.glsl.vert

**Description:** Terrain rendering vertex shader with height-based positioning.

**Inputs:**
- Vertex attributes: position, normal, texCoord

**Outputs:**
- Transformed position
- World position, normal
- Height for biome blending

---

## shaders/terrain.glsl.frag

**Description:** Terrain fragment shader with biome blending based on height and steepness.

**Features:**
- Multiple biome support (up to 5)
- Height-based color blending
- Steepness-based cliff rendering
- Debug visualization modes

**Inputs:**
- World position, normal
- Height value

**Outputs:**
- Biome-blended color with PBR properties

**Uniform Blocks:**
- TerrainUBO: biome parameters, planet radius, debug mode

---

**End of Detailed Index**
