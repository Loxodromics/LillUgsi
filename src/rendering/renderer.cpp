#include "renderer.h"
#include <spdlog/spdlog.h>
#include <SDL3/SDL_vulkan.h>

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

bool Renderer::initialize(SDL_Window* window) {
	SDL_GetWindowSizeInPixels(window, reinterpret_cast<int*>(&this->width), reinterpret_cast<int*>(&this->height));

	if (!this->initializeVulkan()) {
		return false;
	}

	if (!this->createSurface(window)) {
		return false;
	}

	/// Select a physical device
	this->physicalDevice = this->pickPhysicalDevice();
	if (this->physicalDevice == VK_NULL_HANDLE) {
		spdlog::error("Failed to find a suitable GPU");
		return false;
	}

	/// Create logical device
	this->vulkanDevice = std::make_unique<VulkanDevice>();
	std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	if (!this->vulkanDevice->initialize(this->physicalDevice, deviceExtensions)) {
		spdlog::error("Failed to create logical device");
		return false;
	}

	if (!this->createSwapChain()) {
		spdlog::error("Failed to create swap chain");
		return false;
	}

	if (!this->createCommandPool()) {
		spdlog::error("Failed to create command pool");
		return false;
	}

	if (!this->createCommandBuffers()) {
		spdlog::error("Failed to create command buffers");
		return false;
	}

	if (!this->createRenderPass()) {
		spdlog::error("Failed to create render pass");
		return false;
	}

	if (!this->createFramebuffers()) {
		spdlog::error("Failed to create framebuffers");
		return false;
	}

	return true;
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
	/// TODO: Implement actual frame drawing
	/// This will be expanded in future tasks
}

bool Renderer::recreateSwapChain(uint32_t newWidth, uint32_t newHeight) {
	if (this->vulkanDevice) {
		vkDeviceWaitIdle(this->vulkanDevice->getDevice());
	}

	/// Clean up old swap chain resources
	this->cleanupFramebuffers();
	this->vulkanSwapchain.reset();

	/// Recreate swap chain
	this->width = newWidth;
	this->height = newHeight;
	if (!this->createSwapChain()) {
		spdlog::error("Failed to recreate swap chain");
		return false;
	}

	/// Recreate render pass (if necessary)
	/// Note: In most cases, you don't need to recreate the render pass,
	/// but if your render pass configuration depends on the swap chain format,
	/// you might need to recreate it here.

	/// Recreate framebuffers
	if (!this->createFramebuffers()) {
		spdlog::error("Failed to recreate framebuffers");
		return false;
	}

	spdlog::info("Swap chain and framebuffers recreated successfully");
	return true;
}

bool Renderer::isSwapChainAdequate() const {
	/// TODO: Implement proper swap chain adequacy check
	/// This will be expanded in future tasks
	return this->vulkanSwapchain != nullptr;
}

bool Renderer::initializeVulkan() {
	/// Initialize Vulkan
	this->vulkanInstance = std::make_unique<VulkanInstance>();

	/// Get required extensions from SDL
	Uint32 extensionCount = 0;
	const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);

	if (sdlExtensions == nullptr) {
		spdlog::error("Failed to get Vulkan extensions from SDL");
		return false;
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

	if (!this->vulkanInstance->initialize(extensions)) {
		spdlog::error("Failed to initialize Vulkan instance: {}", this->vulkanInstance->getLastError());
		return false;
	}

	return true;
}

bool Renderer::createSurface(SDL_Window* window) {
	if (!SDL_Vulkan_CreateSurface(window, this->vulkanInstance->getInstance(), nullptr, &this->surface)) {
		spdlog::error("Failed to create Vulkan surface");
		return false;
	}
	return true;
}

VkPhysicalDevice Renderer::pickPhysicalDevice() {
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(this->vulkanInstance->getInstance(), &deviceCount, nullptr);

	if (deviceCount == 0) {
		spdlog::error("Failed to find GPUs with Vulkan support");
		return VK_NULL_HANDLE;
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(this->vulkanInstance->getInstance(), &deviceCount, devices.data());

	/// TODO: Implement device selection logic
	/// For now, just pick the first device
	return devices[0];
}

bool Renderer::createSwapChain() {
	this->vulkanSwapchain = std::make_unique<VulkanSwapchain>();
	return this->vulkanSwapchain->initialize(this->physicalDevice, this->vulkanDevice->getDevice(), this->surface, this->width, this->height);
}

bool Renderer::createCommandPool() {
	/// Command pools manage the memory used to store the buffers and command buffers are allocated from them.
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

	/// We want to create command buffers that are associated with the graphics queue family
	poolInfo.queueFamilyIndex = this->vulkanDevice->getGraphicsQueueFamilyIndex();

	/// VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT allows any command buffer allocated from this pool to be individually reset to the initial state
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	VkCommandPool commandPool;
	if (vkCreateCommandPool(this->vulkanDevice->getDevice(), &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
		spdlog::error("Failed to create command pool");
		return false;
	}

	/// Wrap the command pool in our RAII wrapper
	this->commandPool = VulkanCommandPoolHandle(commandPool, [this](VkCommandPool pool) {
		vkDestroyCommandPool(this->vulkanDevice->getDevice(), pool, nullptr);
	});

	spdlog::info("Command pool created successfully");
	return true;
}

bool Renderer::createCommandBuffers() {
	/// We'll create one command buffer for each swap chain image
	uint32_t swapChainImageCount = this->vulkanSwapchain->getSwapChainImages().size();
	this->commandBuffers.resize(swapChainImageCount);

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = this->commandPool.get();

	/// Primary command buffers can be submitted to a queue for execution, but cannot be called from other command buffers
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	allocInfo.commandBufferCount = static_cast<uint32_t>(this->commandBuffers.size());

	/// Allocate the command buffers
	if (vkAllocateCommandBuffers(this->vulkanDevice->getDevice(), &allocInfo, this->commandBuffers.data()) != VK_SUCCESS) {
		spdlog::error("Failed to allocate command buffers");
		return false;
	}

	spdlog::info("Command buffers created successfully");
	return true;
}

bool Renderer::createRenderPass() {
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
	if (vkCreateRenderPass(this->vulkanDevice->getDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
		spdlog::error("Failed to create render pass");
		return false;
	}

	/// Wrap the render pass in our RAII wrapper for automatic resource management
	this->renderPass = VulkanRenderPassHandle(renderPass, [this](VkRenderPass rp) {
		vkDestroyRenderPass(this->vulkanDevice->getDevice(), rp, nullptr);
	});

	spdlog::info("Render pass created successfully");
	return true;
}

bool Renderer::createFramebuffers() {
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
		if (vkCreateFramebuffer(this->vulkanDevice->getDevice(), &framebufferInfo, nullptr, &framebuffer) != VK_SUCCESS) {
			spdlog::error("Failed to create framebuffer for swap chain image {}", i);
			return false;
		}

		/// Store the framebuffer in our vector, wrapped in a VulkanFramebufferHandle for RAII
		this->swapChainFramebuffers[i] = VulkanFramebufferHandle(framebuffer, [this](VkFramebuffer fb) {
			vkDestroyFramebuffer(this->vulkanDevice->getDevice(), fb, nullptr);
		});
	}

	spdlog::info("Created {} framebuffers successfully", this->swapChainFramebuffers.size());
	return true;
}

void Renderer::cleanupFramebuffers() {
	this->swapChainFramebuffers.clear();
	spdlog::info("Framebuffers cleaned up");
}