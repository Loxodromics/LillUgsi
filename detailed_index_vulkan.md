# LillUgsi Vulkan Abstraction - Detailed Index

This file contains detailed information about the Vulkan API abstraction layer classes and functions.

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
