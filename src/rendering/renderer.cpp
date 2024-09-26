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
	/// Note: In most cases, you don't need to recreate the render pass,
	/// but if your render pass configuration depends on the swap chain format,
	/// you might need to recreate it here.
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

bool Renderer::isSwapChainAdequate() const {
	/// TODO: Implement proper swap chain adequacy check
	/// This will be expanded in future tasks
	return this->vulkanSwapchain != nullptr;
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
	/// Describes the format of the vertex data that will be passed to the vertex shader
	/// For now, we're not using any vertex input (hard-coded triangle in vertex shader)
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;

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

		/// Draw command
		/// vkCmdDraw parameters:
		/// 1. Command buffer
		/// 2. Vertex count - we have 3 vertices for our triangle
		/// 3. Instance count - used for instanced rendering, we just have 1 instance
		/// 4. First vertex - offset into the vertex buffer, starts at 0
		/// 5. First instance - used for instanced rendering, starts at 0
		vkCmdDraw(this->commandBuffers[i], 3, 1, 0, 0);

		/// End the render pass
		vkCmdEndRenderPass(this->commandBuffers[i]);

		/// Finish recording the command buffer
		VK_CHECK(vkEndCommandBuffer(this->commandBuffers[i]));
	}

	spdlog::info("Command buffers recorded successfully");
}
