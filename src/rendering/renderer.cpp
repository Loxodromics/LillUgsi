#include "renderer.h"
#include <spdlog/spdlog.h>
#include <SDL3/SDL_vulkan.h>
#include <fstream>

/// Helper function to read a file
static std::vector<char> readFile(const std::string& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		throw std::runtime_error("Failed to open file: " + filename);
	}

	size_t fileSize = (size_t) file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();

	return buffer;
}

Renderer::Renderer()
	: physicalDevice(VK_NULL_HANDLE)
	, surface(VK_NULL_HANDLE)
	, width(0)
	, height(0)
	, isCleanedUp(false) {
}

Renderer::~Renderer() {
	this->cleanup();
}

void Renderer::createLogicalDevice() {
	this->vulkanDevice = std::make_unique<VulkanDevice>();
	std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	this->vulkanDevice->initialize(this->physicalDevice, deviceExtensions);
}

bool Renderer::initialize(SDL_Window* window) {
	SDL_GetWindowSizeInPixels(window, reinterpret_cast<int*>(&this->width), reinterpret_cast<int*>(&this->height));

	try {
		this->initializeVulkan();
		this->createSurface(window);
		this->physicalDevice = this->pickPhysicalDevice();
		this->createLogicalDevice();
		this->createSwapChain();
		this->createCommandPool();
		this->createCommandBuffers();
		this->initializeGeometry();
		this->vulkanBuffer = std::make_unique<VulkanBuffer>(this->vulkanDevice->getDevice(), this->physicalDevice);
		this->createVertexBuffer();
		this->createIndexBuffer();
		this->createRenderPass();
		this->createFramebuffers();
		this->createGraphicsPipeline();
		this->recordCommandBuffers();
		return true;
	}
	catch (const VulkanException& e) {
		spdlog::error("Vulkan error during initialization: {}", e.what());
		return false;
	}
	catch (const std::exception& e) {
		spdlog::error("Error during initialization: {}", e.what());
		return false;
	}
}

void Renderer::cleanup() {
	if (this->isCleanedUp) {
		return;  /// Already cleaned up, do nothing
	}

	/// Ensure all GPU operations are completed before cleanup
	/// This prevents destroying resources that might still be in use by the GPU,
	/// which could lead to crashes or undefined behavior. It's a critical
	/// synchronization point between the CPU and GPU.
	if (this->vulkanDevice) {
		vkDeviceWaitIdle(this->vulkanDevice->getDevice());
	}

	/// Clean up resources in reverse order of creation
	/// Clean up buffers
	if (vertexBuffer) {
		vkDestroyBuffer(this->vulkanDevice->getDevice(), vertexBuffer.get(), nullptr);
	}
	if (vertexBufferMemory != VK_NULL_HANDLE) {
		vkFreeMemory(this->vulkanDevice->getDevice(), vertexBufferMemory, nullptr);
	}
	// if (indexBuffer) {
	// 	vkDestroyBuffer(this->vulkanDevice->getDevice(), indexBuffer.get(), nullptr);
	// }
	// if (indexBufferMemory != VK_NULL_HANDLE) {
	// 	vkFreeMemory(this->vulkanDevice->getDevice(), indexBufferMemory, nullptr);
	// }
	if (this->indexBuffer) {
		this->indexBuffer.reset();
	}
	if (this->indexBufferMemory != VK_NULL_HANDLE) {
		vkFreeMemory(this->vulkanDevice->getDevice(), this->indexBufferMemory, nullptr);
		this->indexBufferMemory = VK_NULL_HANDLE;
	}
	this->vulkanBuffer.reset();
	if (this->cameraBuffer != VK_NULL_HANDLE) {
		vkDestroyBuffer(this->vulkanDevice->getDevice(), this->cameraBuffer, nullptr);
	}
	if (this->cameraBufferMemory != VK_NULL_HANDLE) {
		vkFreeMemory(this->vulkanDevice->getDevice(), this->cameraBufferMemory, nullptr);
	}

	/// Clean up graphics pipeline
	this->graphicsPipeline.reset();
	this->pipelineLayout.reset();

	/// Clean up framebuffers
	this->cleanupFramebuffers();

	/// Clean up render pass
	this->renderPass.reset();

	/// Free command buffers
	if (this->vulkanDevice) {
		if (this->commandPool) {
			vkFreeCommandBuffers(this->vulkanDevice->getDevice(), this->commandPool.get(),
				static_cast<uint32_t>(this->commandBuffers.size()), this->commandBuffers.data());
		} else {
			spdlog::warn("Attempting to free command buffers, but command pool is null");
		}
	} else {
		spdlog::warn("Attempting to free command buffers, but device is null");
	}
	this->commandBuffers.clear();

	/// Reset command pool
	this->commandPool.reset();

	/// Reset swap chain
	this->vulkanSwapchain.reset();

	/// Destroy surface
	if (this->vulkanInstance && this->surface) {
		vkDestroySurfaceKHR(this->vulkanInstance->getInstance(), this->surface, nullptr);
		this->surface = VK_NULL_HANDLE;
	}

	/// Reset device and instance last
	this->vulkanDevice.reset();
	this->vulkanInstance.reset();

	this->isCleanedUp = true;
	spdlog::info("Renderer cleanup completed");
}

void Renderer::drawFrame() {
	// TODO: Implement synchronization primitives (semaphores and fences)
	//       This is a simplified version and won't work correctly without proper synchronization
	if (this->commandBuffers.empty()) {
		spdlog::error("No command buffers available for drawing");
		return;
	}

	uint32_t imageIndex;
	VkResult result = vkAcquireNextImageKHR(
		this->vulkanDevice->getDevice(),
		this->vulkanSwapchain->getSwapChain(),
		UINT64_MAX,
		VK_NULL_HANDLE,
		VK_NULL_HANDLE,
		&imageIndex
	);

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		// Swap chain is out of date (e.g., after a resize)
		// Recreate swap chain and return early
		this->recreateSwapChain(this->width, this->height);
		return;
	} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		spdlog::error("Failed to acquire swap chain image");
		return;
	}

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &this->commandBuffers[imageIndex];

	result = vkQueueSubmit(this->vulkanDevice->getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
	if (result != VK_SUCCESS) {
		spdlog::error("Failed to submit draw command buffer");
		return;
	}

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.swapchainCount = 1;
	VkSwapchainKHR swapChains[] = {this->vulkanSwapchain->getSwapChain()};
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;

	result = vkQueuePresentKHR(this->vulkanDevice->getPresentQueue(), &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		this->recreateSwapChain(this->width, this->height);
	} else if (result != VK_SUCCESS) {
		spdlog::error("Failed to present swap chain image");
	}

	// TODO: Implement proper frame synchronization
	// Wait for the GPU to finish its work
	vkQueueWaitIdle(this->vulkanDevice->getPresentQueue());
}

bool Renderer::recreateSwapChain(uint32_t newWidth, uint32_t newHeight) {
	try {
		if (this->vulkanDevice) {
			vkDeviceWaitIdle(this->vulkanDevice->getDevice());
		}

		/// Clean up old swap chain resources
		this->cleanupFramebuffers();
		this->vulkanSwapchain.reset();

		/// Free old command buffers
		if (this->vulkanDevice && !this->commandBuffers.empty()) {
			vkFreeCommandBuffers(
				this->vulkanDevice->getDevice(),
				this->commandPool.get(),
				static_cast<uint32_t>(this->commandBuffers.size()),
				this->commandBuffers.data()
			);
			this->commandBuffers.clear();
		}

		/// Recreate swap chain
		this->width = newWidth;
		this->height = newHeight;

		/// Recreate render pass (if necessary)
		/// Note: In most cases, we don't need to recreate the render pass,
		/// but if our render pass configuration depends on the swap chain format,
		/// we might need to recreate it here.

		this->createSwapChain();

		/// Recreate framebuffers
		this->createFramebuffers();

		/// Recreate command buffers
		this->recordCommandBuffers();

		spdlog::info("Swap chain, framebuffers, and command buffers recreated successfully");
		return true;
	}
	catch (const VulkanException& e) {
		spdlog::error("Vulkan error during swap chain recreation: {}", e.what());
		return false;
	}
	catch (const std::exception& e) {
		spdlog::error("Error during swap chain recreation: {}", e.what());
		return false;
	}
}

void Renderer::initializeVulkan() {
	/// Initialize Vulkan
	this->vulkanInstance = std::make_unique<VulkanInstance>();

	/// Get required extensions from SDL
	Uint32 extensionCount = 0;
	const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);

	if (sdlExtensions == nullptr) {
		throw VulkanException(VK_ERROR_EXTENSION_NOT_PRESENT, "Failed to get Vulkan extensions from SDL", __FUNCTION__, __FILE__, __LINE__);
	}

	/// Copy SDL extensions and add any additional required extensions
	std::vector<const char*> extensions(sdlExtensions, sdlExtensions + extensionCount);

	/// Add VK_EXT_debug_utils extension if you want to use validation layers
	extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

	/// Log available Vulkan extensions
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

	spdlog::info("Available Vulkan extensions:");
	for (const auto& extension : availableExtensions) {
		spdlog::info("  {}", extension.extensionName);
	}

	spdlog::info("Required Vulkan extensions:");
	for (const auto& extension : extensions) {
		spdlog::info("  {}", extension);
	}

	this->vulkanInstance->initialize(extensions);

	spdlog::info("Vulkan initialized successfully");
}

void Renderer::createSurface(SDL_Window* window) {
	VkSurfaceKHR surface;
	if (!SDL_Vulkan_CreateSurface(window, this->vulkanInstance->getInstance(), nullptr, &surface)) {
		throw VulkanException(VK_ERROR_INITIALIZATION_FAILED, "Failed to create Vulkan surface", __FUNCTION__, __FILE__, __LINE__);
	}
	this->surface = surface;
	spdlog::info("Vulkan surface created successfully");
}

VkPhysicalDevice Renderer::pickPhysicalDevice() {
	uint32_t deviceCount = 0;
	VK_CHECK(vkEnumeratePhysicalDevices(this->vulkanInstance->getInstance(), &deviceCount, nullptr));

	if (deviceCount == 0) {
		throw VulkanException(VK_ERROR_INITIALIZATION_FAILED, "Failed to find GPUs with Vulkan support", __FUNCTION__, __FILE__, __LINE__);
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	VK_CHECK(vkEnumeratePhysicalDevices(this->vulkanInstance->getInstance(), &deviceCount, devices.data()));

	/// TODO: Implement device selection logic
	/// For now, just pick the first device
	spdlog::info("Physical device selected successfully");
	return devices[0];
}

void Renderer::createSwapChain() {
	this->vulkanSwapchain = std::make_unique<VulkanSwapchain>();
	this->vulkanSwapchain->initialize(this->physicalDevice, this->vulkanDevice->getDevice(), this->surface, this->width, this->height);
}

void Renderer::createCommandPool() {
	/// Command pools manage the memory used to store the buffers and command buffers are allocated from them.
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

	/// We want to create command buffers that are associated with the graphics queue family
	poolInfo.queueFamilyIndex = this->vulkanDevice->getGraphicsQueueFamilyIndex();

	/// VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT allows any command buffer allocated from this pool to be individually reset to the initial state
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	VkCommandPool commandPool;
	VK_CHECK(vkCreateCommandPool(this->vulkanDevice->getDevice(), &poolInfo, nullptr, &commandPool));

	/// Wrap the command pool in our RAII wrapper
	this->commandPool = VulkanCommandPoolHandle(commandPool, [this](VkCommandPool pool) {
		vkDestroyCommandPool(this->vulkanDevice->getDevice(), pool, nullptr);
	});

	spdlog::info("Command pool created successfully");
}

void Renderer::createCommandBuffers() {
	/// We'll create one command buffer for each swap chain image
	uint32_t swapChainImageCount = this->vulkanSwapchain->getSwapChainImages().size();
	this->commandBuffers.resize(swapChainImageCount);

	/// Set up command buffer allocation info
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = this->commandPool.get();

	/// Primary command buffers can be submitted to a queue for execution, but cannot be called from other command buffers
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	allocInfo.commandBufferCount = static_cast<uint32_t>(this->commandBuffers.size());

	/// Allocate the command buffers
	VK_CHECK(vkAllocateCommandBuffers(this->vulkanDevice->getDevice(), &allocInfo, this->commandBuffers.data()));

	spdlog::info("Command buffers created successfully");
}

void Renderer::createRenderPass() {
	/// VkAttachmentDescription: Describes a framebuffer attachment (e.g., color, depth, or stencil buffer).
	/// It defines how the attachment will be used throughout the render pass.
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = this->vulkanSwapchain->getSwapChainImageFormat();
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; /// No multisampling
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; /// Clear the attachment at the start of the render pass
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; /// Store the result for later use (e.g., presentation)
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; /// We're not using stencil buffer
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; /// We don't care about the initial layout
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; /// The image will be presented in the swap chain

	/// VkAttachmentReference: Describes how a specific attachment will be used in a subpass.
	/// It links the attachment description to a specific subpass.
	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0; /// Index of the attachment in the attachment descriptions array
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; /// Layout to use during the subpass

	/// VkSubpassDescription: Describes a single subpass in the render pass.
	/// A subpass is a set of rendering operations that can be executed together efficiently.
	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; /// This is a graphics subpass
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	/// VkSubpassDependency: Defines dependencies between subpasses.
	/// This is crucial for handling synchronization between subpasses and with external operations.
	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL; /// Dependency on operations outside the render pass
	dependency.dstSubpass = 0; /// Our subpass index
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; /// Stage of the dependency in the source subpass
	dependency.srcAccessMask = 0; /// No access in the source subpass
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; /// Stage of the dependency in the destination subpass
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; /// We'll be writing to the color attachment

	/// VkRenderPassCreateInfo: Aggregates all the information needed to create a render pass.
	/// It includes attachment descriptions, subpasses, and dependencies.
	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	VkRenderPass renderPass;
	VK_CHECK(vkCreateRenderPass(this->vulkanDevice->getDevice(), &renderPassInfo, nullptr, &renderPass));

	/// Wrap the render pass in our RAII wrapper for automatic resource management
	this->renderPass = VulkanRenderPassHandle(renderPass, [this](VkRenderPass rp) {
		vkDestroyRenderPass(this->vulkanDevice->getDevice(), rp, nullptr);
	});

	spdlog::info("Render pass created successfully");
}

void Renderer::createFramebuffers() {
	/// Framebuffers are the destination for the rendering operations.
	/// We create one framebuffer for each image view in the swap chain.

	/// Resize the framebuffer vector to match the number of swap chain images
	this->swapChainFramebuffers.resize(this->vulkanSwapchain->getSwapChainImageViews().size());

	/// Iterate through each swap chain image view and create a framebuffer for it
	for (size_t i = 0; i < this->vulkanSwapchain->getSwapChainImageViews().size(); i++) {
		/// We'll use only one attachment for now - the color attachment
		VkImageView attachments[] = {
			this->vulkanSwapchain->getSwapChainImageViews()[i].get()
		};

		/// Create the framebuffer create info structure
		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = this->renderPass.get(); /// The render pass this framebuffer is compatible with
		framebufferInfo.attachmentCount = 1; /// Number of attachments (just color for now)
		framebufferInfo.pAttachments = attachments; /// Pointer to the attachments array
		framebufferInfo.width = this->vulkanSwapchain->getSwapChainExtent().width;
		framebufferInfo.height = this->vulkanSwapchain->getSwapChainExtent().height;
		framebufferInfo.layers = 1; /// Number of layers in image arrays

		/// Create the framebuffer
		VkFramebuffer framebuffer;
		VK_CHECK(vkCreateFramebuffer(this->vulkanDevice->getDevice(), &framebufferInfo, nullptr, &framebuffer));

		/// Store the framebuffer in our vector, wrapped in a VulkanFramebufferHandle for RAII
		this->swapChainFramebuffers[i] = VulkanFramebufferHandle(framebuffer, [this](VkFramebuffer fb) {
			vkDestroyFramebuffer(this->vulkanDevice->getDevice(), fb, nullptr);
		});
	}

	spdlog::info("Created {} framebuffers successfully", this->swapChainFramebuffers.size());
}

void Renderer::cleanupFramebuffers() {
	this->swapChainFramebuffers.clear();
	spdlog::info("Framebuffers cleaned up");
}

VulkanShaderModuleHandle Renderer::createShaderModule(const std::vector<char>& code) {
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(this->vulkanDevice->getDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		spdlog::error("Failed to create shader module");
		return VulkanShaderModuleHandle();
	}

	return VulkanShaderModuleHandle(shaderModule, [this](VkShaderModule sm) {
		vkDestroyShaderModule(this->vulkanDevice->getDevice(), sm, nullptr);
	});
}

void Renderer::createGraphicsPipeline() {
	/// Read shader files
	/// Shader code is precompiled into SPIR-V format using glslc
	auto vertShaderCode = readFile("shaders/vert.spv");
	auto fragShaderCode = readFile("shaders/frag.spv");

	/// Create shader modules
	/// Shader modules are a thin wrapper around the shader bytecode
	VulkanShaderModuleHandle vertShaderModule = this->createShaderModule(vertShaderCode);
	VulkanShaderModuleHandle fragShaderModule = this->createShaderModule(fragShaderCode);

	/// Set up shader stage creation information
	/// This describes which shader is used for which pipeline stage
	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;  /// This is the vertex shader stage
	vertShaderStageInfo.module = vertShaderModule.get();
	vertShaderStageInfo.pName = "main";  /// The entry point of the shader

	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;  /// This is the fragment shader stage
	fragShaderStageInfo.module = fragShaderModule.get();
	fragShaderStageInfo.pName = "main";  /// The entry point of the shader

	VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

	/// Vertex input state
	/// This describes the format of the vertex data that will be passed to the vertex shader
	VkVertexInputBindingDescription bindingDescription{};
	bindingDescription.binding = 0; /// We're using a single vertex buffer, so we use binding 0
	bindingDescription.stride = sizeof(Vertex); /// Size of each vertex
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; /// Move to the next data entry after each vertex

	/// Attribute descriptions
	/// These describe how to extract vertex attributes from the vertex buffer data
	std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

	/// Position attribute
	attributeDescriptions[0].binding = 0; /// Which binding the per-vertex data comes from
	attributeDescriptions[0].location = 0; /// Location in the vertex shader
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT; /// Format of the attribute (vec3)
	attributeDescriptions[0].offset = offsetof(Vertex, pos); /// Offset of the attribute in the vertex struct

	/// Color attribute
	attributeDescriptions[1].binding = 0;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[1].offset = offsetof(Vertex, color);

	/// Vertex input state create info
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	/// Input assembly state
	/// Describes how to assemble vertices into primitives
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;  /// Treat each three vertices as a triangle
	inputAssembly.primitiveRestartEnable = VK_FALSE;  /// Don't use primitive restart

	/// Viewport and scissor
	/// The viewport describes the region of the framebuffer that the output will be rendered to
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(this->vulkanSwapchain->getSwapChainExtent().width);
	viewport.height = static_cast<float>(this->vulkanSwapchain->getSwapChainExtent().height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	/// The scissor rectangles define in which regions pixels will actually be stored
	/// Any pixels outside the scissor rectangles will be discarded
	VkRect2D scissor{};
	scissor.offset = {0, 0};
	scissor.extent = this->vulkanSwapchain->getSwapChainExtent();

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	/// Rasterizer
	/// The rasterizer takes the geometry that is shaped by the vertices from the vertex shader
	/// and turns it into fragments to be colored by the fragment shader
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;  /// Don't clamp fragments to near and far planes
	rasterizer.rasterizerDiscardEnable = VK_FALSE;  /// Don't discard all primitives before rasterization stage
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;  /// Fill the area of the polygon with fragments
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;  /// Cull back faces
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;  /// Specify vertex order for faces to be considered front-facing
	rasterizer.depthBiasEnable = VK_FALSE;  /// Don't use depth bias

	/// Multisampling
	/// This is one of the ways to perform anti-aliasing
	/// We're not using it now, so we'll just disable it
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	/// Color blending
	/// After a fragment shader has returned a color, it needs to be combined with the color
	/// that is already in the framebuffer. This is called color blending.
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;  /// Disable blending for now

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	/// Pipeline layout
	/// Specifies the uniform values and push constants that can be used in shaders
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	/// We're not using any descriptor sets or push constants yet, so we'll leave these as default

	VkPipelineLayout pipelineLayout;
	VK_CHECK(vkCreatePipelineLayout(this->vulkanDevice->getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout));

	this->pipelineLayout = VulkanPipelineLayoutHandle(pipelineLayout, [this](VkPipelineLayout pl) {
		vkDestroyPipelineLayout(this->vulkanDevice->getDevice(), pl, nullptr);
	});

	/// Create the graphics pipeline
	/// This brings together all of the structures we've created so far
	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;  /// Vertex and fragment shader stages
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.layout = this->pipelineLayout.get();
	pipelineInfo.renderPass = this->renderPass.get();
	pipelineInfo.subpass = 0;  /// Index of the subpass in the render pass where this pipeline will be used
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;  /// Used for pipeline derivatives, not used here

	VkPipeline graphicsPipeline;
	VK_CHECK(vkCreateGraphicsPipelines(this->vulkanDevice->getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline));

	/// Wrap the pipeline in our RAII wrapper
	this->graphicsPipeline = VulkanPipelineHandle(graphicsPipeline, [this](VkPipeline gp) {
		vkDestroyPipeline(this->vulkanDevice->getDevice(), gp, nullptr);
	});

	spdlog::info("Graphics pipeline created successfully");
}

void Renderer::recordCommandBuffers() {
	/// Resize command buffers vector to match the number of framebuffers
	/// We need one command buffer for each swap chain image
	this->commandBuffers.resize(this->swapChainFramebuffers.size());

	/// Set up command buffer allocation info
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = this->commandPool.get();
	/// Primary command buffers can be submitted directly to queues
	/// Secondary command buffers can only be called from primary command buffers
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = static_cast<uint32_t>(this->commandBuffers.size());

	/// Allocate command buffers from the command pool
	VK_CHECK(vkAllocateCommandBuffers(this->vulkanDevice->getDevice(), &allocInfo, this->commandBuffers.data()));

	/// Record commands for each framebuffer
	for (size_t i = 0; i < this->commandBuffers.size(); i++) {
		/// Start command buffer recording
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		/// VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT allows the command buffer to be resubmitted while it is also already pending execution
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		VK_CHECK(vkBeginCommandBuffer(this->commandBuffers[i], &beginInfo));

		/// Begin the render pass
		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = this->renderPass.get();
		renderPassInfo.framebuffer = this->swapChainFramebuffers[i].get();
		/// Define the render area, typically the size of the framebuffer
		renderPassInfo.renderArea.offset = {0, 0};
		renderPassInfo.renderArea.extent = this->vulkanSwapchain->getSwapChainExtent();

		/// Define clear values for the attachments
		/// This is the color the screen will be cleared to at the start of the render pass
		VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};  // Black with 100% opacity
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearColor;

		/// Begin the render pass
		/// VK_SUBPASS_CONTENTS_INLINE means the render pass commands will be embedded in the primary command buffer
		/// and no secondary command buffers will be executed
		vkCmdBeginRenderPass(this->commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		/// Bind the graphics pipeline
		vkCmdBindPipeline(this->commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, this->graphicsPipeline.get());

		/// Bind vertex buffer
		VkBuffer vertexBuffers[] = {this->vertexBuffer.get()};
		VkDeviceSize offsets[] = {0};
		vkCmdBindVertexBuffers(this->commandBuffers[i], 0, 1, vertexBuffers, offsets);

		/// Bind index buffer
		vkCmdBindIndexBuffer(this->commandBuffers[i], this->indexBuffer.get(), 0, VK_INDEX_TYPE_UINT16);

		/// Draw command
		/// vkCmdDrawIndexed parameters:
		/// 1. Command buffer
		/// 2. Index count - number of indices to draw
		/// 3. Instance count - used for instanced rendering, we just have 1 instance
		/// 4. First index - offset into the index buffer, starts at 0
		/// 5. Vertex offset - used as a bias to the vertex index, 0 in our case
		/// 6. First instance - used for instanced rendering, starts at 0
		vkCmdDrawIndexed(this->commandBuffers[i], static_cast<uint32_t>(this->indices.size()), 1, 0, 0, 0);

		/// End the render pass
		vkCmdEndRenderPass(this->commandBuffers[i]);

		/// Finish recording the command buffer
		VK_CHECK(vkEndCommandBuffer(this->commandBuffers[i]));
	}

	spdlog::info("Command buffers recorded successfully");
}

void Renderer::createCameraUniformBuffer() {
	VkDeviceSize bufferSize = sizeof(CameraUBO);

	/// Create a staging buffer for initial data transfer
	VulkanBufferHandle stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	try {
		/// Create staging buffer
		/// TODO: use VK_CHECK
		this->vulkanBuffer->createBuffer(
			bufferSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer,
			stagingBufferMemory
		);

		/// Create the actual uniform buffer
		this->vulkanBuffer->createBuffer(
			bufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			this->cameraBuffer,
			this->cameraBufferMemory
		);

		/// Initialize buffer with identity matrices
		CameraUBO initialData{};
		initialData.view = glm::mat4(1.0f);
		initialData.projection = glm::mat4(1.0f);

		/// Copy initial data to staging buffer
		void* data;
		VK_CHECK(vkMapMemory(this->vulkanDevice->getDevice(), stagingBufferMemory, 0, bufferSize, 0, &data));
		memcpy(data, &initialData, bufferSize);
		vkUnmapMemory(this->vulkanDevice->getDevice(), stagingBufferMemory);

		/// Copy data from staging buffer to uniform buffer
		this->vulkanBuffer->copyBuffer(
			this->commandPool.get(),
			this->vulkanDevice->getGraphicsQueue(),
			stagingBuffer.get(),
			this->cameraBuffer.get(),
			bufferSize
		);

		/// Clean up staging buffer
		vkDestroyBuffer(this->vulkanDevice->getDevice(), stagingBuffer.get(), nullptr);
		vkFreeMemory(this->vulkanDevice->getDevice(), stagingBufferMemory, nullptr);

	} catch (const VulkanException& e) {
		/// Clean up any resources that were created before the exception
		if (stagingBuffer.get() != VK_NULL_HANDLE) {
			vkDestroyBuffer(this->vulkanDevice->getDevice(), stagingBuffer.get(), nullptr);
		}
		if (stagingBufferMemory != VK_NULL_HANDLE) {
			vkFreeMemory(this->vulkanDevice->getDevice(), stagingBufferMemory, nullptr);
		}
		throw; /// Re-throw the exception after cleanup
	}

	spdlog::info("Camera uniform buffer created successfully");
}

void Renderer::updateCameraUniformBuffer() {
	/// This method will be called each frame to update the camera data
	/// For now, we'll just use identity matrices
	CameraUBO ubo{};
	ubo.view = glm::mat4(1.0f);
	ubo.projection = glm::mat4(1.0f);

	/// When we implement the camera class, we'll update these matrices like this:
	/// ubo.view = this->camera->getViewMatrix();
	/// ubo.projection = this->camera->getProjectionMatrix();

	/// Copy the new data to the uniform buffer
	void* data;
	VK_CHECK(vkMapMemory(this->vulkanDevice->getDevice(), this->cameraBufferMemory, 0, sizeof(ubo), 0, &data));
	memcpy(data, &ubo, sizeof(ubo));
	vkUnmapMemory(this->vulkanDevice->getDevice(), this->cameraBufferMemory);
}

void Renderer::createVertexBuffer() {
	/// Calculate the size of the buffer we need
	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

	/// We use a staging buffer for several reasons:
	/// 1. It allows us to use a memory type that is host visible (CPU can write to it)
	/// 2. We can then transfer this to a device local memory, which is faster for the GPU to read from
	/// This two-step process is often faster than using a buffer that is both host visible and device local,
	/// especially on discrete GPUs where device local memory is separate from system memory
	VulkanBufferHandle stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	/// Create a staging buffer that is host visible and host coherent
	/// Host visible allows us to map it to CPU memory
	/// Host coherent means we don't need to explicitly flush writes to the GPU
	this->vulkanBuffer->createBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,  /// This buffer will be used as the source in a memory transfer operation
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory
	);

	/// Map the staging buffer to CPU memory
	/// This allows us to write our vertex data directly to the buffer
	void* data;
	VK_CHECK(vkMapMemory(this->vulkanDevice->getDevice(), stagingBufferMemory, 0, bufferSize, 0, &data));

	/// Copy our vertex data to the mapped memory
	/// memcpy is used here for simplicity, but for larger data sets or more complex scenarios,
	/// we might want to consider more sophisticated methods of populating our vertex buffer
	memcpy(data, vertices.data(), (size_t) bufferSize);

	/// Unmap the memory
	/// We don't need to call vkFlushMappedMemoryRanges because we used a coherent memory type
	vkUnmapMemory(this->vulkanDevice->getDevice(), stagingBufferMemory);

	/// Now create the actual vertex buffer
	/// This buffer will be in device local memory, which is ideal for the GPU to read from
	this->vulkanBuffer->createBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,  /// This buffer will be the destination of a transfer and used as a vertex buffer
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,  /// Device local memory is the fastest memory type for the GPU
		this->vertexBuffer,
		this->vertexBufferMemory
	);

	/// Copy the data from the staging buffer to the vertex buffer
	/// This transfers the data from host visible memory to device local memory
	this->vulkanBuffer->copyBuffer(
		this->commandPool.get(),
		this->vulkanDevice->getGraphicsQueue(),
		stagingBuffer.get(),
		this->vertexBuffer.get(),
		bufferSize
	);

	/// Clean up the staging buffer and its memory
	/// It's important to do this explicitly to ensure proper resource management
	/// The staging buffer was only needed to transfer the data to the device local vertex buffer
	stagingBuffer.reset();
	vkFreeMemory(this->vulkanDevice->getDevice(), stagingBufferMemory, nullptr);

	/// Note: We keep the vertex buffer (this->vertexBuffer) around because we'll need it for rendering
	/// It will be cleaned up in the Renderer's cleanup method

	spdlog::info("Vertex buffer created successfully. Size: {}", bufferSize);
}

void Renderer::createIndexBuffer() {
	/// Calculate the size of the index buffer
	/// This is the total size of all indices in bytes
	VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

	/// We use a staging buffer for the same reasons as in createVertexBuffer:
	/// 1. It allows us to use CPU-accessible memory for initial data transfer
	/// 2. We can then move this data to high-performance GPU memory
	/// This two-step process is often more efficient, especially on discrete GPUs
	VulkanBufferHandle stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	/// Create a staging buffer that is host visible and host coherent
	/// Host visible allows CPU to write to it, host coherent avoids manual flushing
	this->vulkanBuffer->createBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,  /// This buffer will be the source in a memory transfer
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory
	);

	/// Map the staging buffer memory to a CPU accessible pointer
	void* data;
	VK_CHECK(vkMapMemory(this->vulkanDevice->getDevice(), stagingBufferMemory, 0, bufferSize, 0, &data));

	/// Copy our index data to the mapped memory
	/// For larger datasets, you might want to consider using a more sophisticated
	/// method of populating your buffer
	memcpy(data, indices.data(), (size_t) bufferSize);

	/// Unmap the memory
	/// No need to manually flush due to the coherent memory type
	vkUnmapMemory(this->vulkanDevice->getDevice(), stagingBufferMemory);

	/// Create the actual index buffer
	/// This will reside in device local memory for optimal GPU performance
	this->vulkanBuffer->createBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,  /// Will be the destination of a transfer and used as an index buffer
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,  /// Device local memory is fastest for GPU read operations
		this->indexBuffer,
		this->indexBufferMemory
	);

	/// Transfer the data from the staging buffer to the index buffer
	/// This moves the data from CPU-accessible memory to high-performance GPU memory
	this->vulkanBuffer->copyBuffer(
		this->commandPool.get(),
		this->vulkanDevice->getGraphicsQueue(),
		stagingBuffer.get(),
		this->indexBuffer.get(),
		bufferSize
	);

	/// Clean up the staging buffer and its memory
	/// This step is crucial for proper resource management
	/// The staging buffer has served its purpose and is no longer needed
	stagingBuffer.reset();
	vkFreeMemory(this->vulkanDevice->getDevice(), stagingBufferMemory, nullptr);

	/// Note: We keep the index buffer (this->indexBuffer) as it will be used for rendering
	/// It will be properly cleaned up in the Renderer's cleanup method

	spdlog::info("Index buffer created successfully. Size: {}", bufferSize);
}

void Renderer::initializeGeometry() {
	vertices = {
		{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
		{{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
		{{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
		{{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}}
	};

	indices = {
		0, 1, 2, 2, 3, 0
	};
}