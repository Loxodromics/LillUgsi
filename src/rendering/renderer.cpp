#include "renderer.h"
#include "rendering/cubemesh.h"
#include "rendering/icospheremesh.h"
#include "vulkan/vertexbuffer.h"
#include "vulkan/indexbuffer.h"
#include <glm/gtc/matrix_transform.hpp>
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

namespace lillugsi::rendering {

Renderer::Renderer()
	: vulkanContext(std::make_unique<vulkan::VulkanContext>())
	, width(0)
	, height(0)
	, isCleanedUp(false) {
	/// We create the scene first as it's fundamental to the renderer's operation
	/// This ensures the scene graph exists before any rendering setup
	this->scene = std::make_unique<scene::Scene>();

	/// Initialize the camera with a default position
	/// We place the camera slightly back and up to view the scene
	this->camera = std::make_unique<EditorCamera>(glm::vec3(0.0f, 2.0f, 5.0f));
}

Renderer::~Renderer() {
	this->cleanup();
}

bool Renderer::initialize(SDL_Window* window) {
	try {
		/// Initialize Vulkan context
		if (!this->vulkanContext->initialize(window)) {
			spdlog::error("Failed to initialize Vulkan context");
			return false;
		}

		/// Get window size
		SDL_GetWindowSizeInPixels(window, reinterpret_cast<int*>(&this->width),
			reinterpret_cast<int*>(&this->height));

		/// Initialize depth buffer
		this->initializeDepthBuffer();

		/// Create render pass
		this->createRenderPass();

		/// Create command pool
		this->createCommandPool();

		/// Initialize pipeline manager
		/// This needs to happen before materials are created
		/// as they depend on the global descriptor layouts
		this->pipelineManager = std::make_unique<vulkan::PipelineManager>(
			this->vulkanContext->getDevice()->getDevice(),
			this->renderPass.get()
		);
		this->pipelineManager->initialize();

		/// Initialize buffer utility after Vulkan setup
		this->vulkanBufferUtility = std::make_unique<vulkan::VulkanBuffer>(
			this->vulkanContext->getDevice()->getDevice(),
			this->vulkanContext->getPhysicalDevice()
		);

		/// Create framebuffers
		this->createFramebuffers();

		/// Initialize MeshManager
		this->meshManager = std::make_unique<MeshManager>(
			this->vulkanContext->getDevice()->getDevice(),
			this->vulkanContext->getPhysicalDevice(),
			this->vulkanContext->getDevice()->getGraphicsQueue(),
			this->vulkanContext->getDevice()->getGraphicsQueueFamilyIndex()
		);

		/// Create camera uniform buffer
		this->createCameraUniformBuffer();

		/// Initialize light management and buffer
		this->lightManager = std::make_unique<LightManager>();
		this->createLightUniformBuffer();

		/// Create descriptor pool
		this->createDescriptorPool();

		/// Create descriptor sets
		/// Using global layouts from pipeline manager
		this->createDescriptorSets();

		/// Initialize material system
		this->initializeMaterials();

		/// Initialize the scene
		this->initializeScene();

		/// Create command buffers
		this->createCommandBuffers();

		/// Record command buffers
		this->recordCommandBuffers();

		/// Create synchronization objects
		this->createSyncObjects();

		/// Initialize camera with default position
		this->camera = std::make_unique<EditorCamera>(glm::vec3(0.0f, 0.0f, 5.0f));

		spdlog::info("Renderer initialized successfully");
		return true;
	}
	catch (const vulkan::VulkanException& e) {
		spdlog::error("Vulkan error during renderer initialization: {}", e.what());
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
	if (this->vulkanContext) {
		vkDeviceWaitIdle(this->vulkanContext->getDevice()->getDevice());
	}

	/// Clean up light resources
	if (this->lightBuffer) {
		this->lightBuffer.reset();
	}
	if (this->lightBufferMemory != VK_NULL_HANDLE) {
		vkFreeMemory(this->vulkanContext->getDevice()->getDevice(),
			this->lightBufferMemory, nullptr);
		this->lightBufferMemory = VK_NULL_HANDLE;
	}
	this->lightManager.reset();

	/// Clean up materials before scene
	this->materialManager.reset();

	this->camera.reset();

	/// Clean up scene first as it might hold GPU resources
	/// This ensures proper cleanup order and avoids dangling references
	this->scene.reset();

	/// Clean up synchronization objects
	this->cleanupSyncObjects();

	/// Free command buffers
	if (this->vulkanContext && this->commandPool) {
		vkFreeCommandBuffers(this->vulkanContext->getDevice()->getDevice(), this->commandPool.get(),
							 static_cast<uint32_t>(this->commandBuffers.size()), this->commandBuffers.data());
	}
	this->commandBuffers.clear();

	/// Clean up graphics pipeline
	this->graphicsPipeline.reset();
	this->pipelineLayout.reset();
	this->pipelineManager->cleanup();

	/// Clean up descriptor pool and layout
	if (this->descriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(this->vulkanContext->getDevice()->getDevice(), this->descriptorPool, nullptr);
		this->descriptorPool = VK_NULL_HANDLE;
	}

	/// Clean up uniform buffers
	if (this->cameraBuffer) {
		this->cameraBuffer.reset();
	}
	if (this->cameraBufferMemory != VK_NULL_HANDLE) {
		vkFreeMemory(this->vulkanContext->getDevice()->getDevice(), this->cameraBufferMemory, nullptr);
		this->cameraBufferMemory = VK_NULL_HANDLE;
	}

	this->vulkanBufferUtility.reset();

	/// Clean up mesh manager before other resources
	/// This ensures buffers are destroyed before the device
	if (this->meshManager) {
		this->meshManager->cleanup();
		this->meshManager.reset();
	}

	this->vulkanBufferUtility.reset();

	/// Reset command pool
	this->commandPool.reset();

	/// Clean up framebuffers
	this->cleanupFramebuffers();

	/// Clean up depth buffer
	this->depthBuffer.reset();

	/// Clean up render pass
	this->renderPass.reset();

	/// Clean up Vulkan context (this will handle swap chain, device, and instance cleanup)
	this->vulkanContext.reset();

	this->isCleanedUp = true;
	spdlog::info("Renderer cleanup completed");
}

void Renderer::drawFrame() {
	/// Wait for the previous frame to finish
	/// This ensures that we're not using resources that may still be in use by the GPU
	VK_CHECK(vkWaitForFences(this->vulkanContext->getDevice()->getDevice(), 1, &this->inFlightFence, VK_TRUE, UINT64_MAX));

	/// Reset the fence to the unsignaled state for use in the current frame
	VK_CHECK(vkResetFences(this->vulkanContext->getDevice()->getDevice(), 1, &this->inFlightFence));

	/// Acquire an image from the swap chain
	uint32_t imageIndex;
	VkResult result = vkAcquireNextImageKHR(
		this->vulkanContext->getDevice()->getDevice(),
		this->vulkanContext->getSwapChain()->getSwapChain(),
		UINT64_MAX, /// Disable timeout
		this->imageAvailableSemaphore, /// Semaphore to signal when the image is available
		VK_NULL_HANDLE,
		&imageIndex
	);

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		/// Swap chain is out of date (e.g., after a resize)
		/// Recreate swap chain and return early
		this->recreateSwapChain(this->width, this->height);
		return;
	} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		throw vulkan::VulkanException(result, "Failed to acquire swap chain image", __FUNCTION__, __FILE__, __LINE__);
	}

	/// Update uniform buffer with current camera data
	this->updateCameraUniformBuffer();

	this->updateLightUniformBuffer();

	/// Record command buffers with current scene state
	this->recordCommandBuffers();

	/// Set up the submit info struct
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	/// Configure pipeline stage flags
	/// We want to wait on the color attachment output stage before we start writing colors
	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &this->imageAvailableSemaphore;
	submitInfo.pWaitDstStageMask = waitStages;

	/// Set up the command buffer to submit
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &this->commandBuffers[imageIndex];

	/// Set up the semaphore to signal when rendering is finished
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &this->renderFinishedSemaphore;

	/// Submit the command buffer
	VK_CHECK(vkQueueSubmit(this->vulkanContext->getDevice()->getGraphicsQueue(), 1, &submitInfo, this->inFlightFence));

	/// Set up the present info struct
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &this->renderFinishedSemaphore;

	VkSwapchainKHR swapChains[] = {this->vulkanContext->getSwapChain()->getSwapChain()};
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;

	/// Present the image to the screen
	result = vkQueuePresentKHR(this->vulkanContext->getDevice()->getPresentQueue(), &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		this->recreateSwapChain(this->width, this->height);
	} else if (result != VK_SUCCESS) {
		throw vulkan::VulkanException(result, "Failed to present swap chain image", __FUNCTION__, __FILE__, __LINE__);
	}
}

void Renderer::update(float deltaTime) {
	/// Store frame time for effects and animations
	this->currentFrameTime = deltaTime;

	/// Update scene with the provided delta time
	/// This ensures all scene objects use the same time step
	this->scene->update(deltaTime);

	/// Update camera with the same time step
	/// Camera movement and transitions use game-scaled time
	this->camera->update(deltaTime);
}

bool Renderer::recreateSwapChain(uint32_t newWidth, uint32_t newHeight) {
	try {
		if (this->vulkanContext->getDevice()) {
			vkDeviceWaitIdle(this->vulkanContext->getDevice()->getDevice());
		}

		/// Clean up old swap chain resources
		this->cleanupFramebuffers();

		/// Free old command buffers
		if (this->vulkanContext->getDevice() && !this->commandBuffers.empty()) {
			vkFreeCommandBuffers(
				this->vulkanContext->getDevice()->getDevice(),
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

		this->vulkanContext->createSwapChain(this->width, this->height);

		/// Recreate framebuffers
		this->createFramebuffers();

		/// Recreate command buffers
		this->recordCommandBuffers();

		/// Update camera aspect ratio
		/// This ensures proper projection after resize
		float aspectRatio = static_cast<float>(this->width) / static_cast<float>(this->height);
		this->camera->getProjectionMatrix(aspectRatio);

		spdlog::info("Swap chain recreated with dimensions {}x{}",  this->width, this->height);
		return true;
	}
	catch (const vulkan::VulkanException& e) {
		spdlog::error("Vulkan error during swap chain recreation: {}", e.what());
		return false;
	}
	catch (const std::exception& e) {
		spdlog::error("Error during swap chain recreation: {}", e.what());
		return false;
	}
}

void Renderer::createCommandPool() {
	/// Command pools manage the memory used to store the buffers and command buffers are allocated from them.
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

	/// We want to create command buffers that are associated with the graphics queue family
	poolInfo.queueFamilyIndex = this->vulkanContext->getDevice()->getGraphicsQueueFamilyIndex();

	/// VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT allows any command buffer allocated from this pool to be individually reset to the initial state
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	VkCommandPool commandPool;
	VK_CHECK(vkCreateCommandPool(this->vulkanContext->getDevice()->getDevice(), &poolInfo, nullptr, &commandPool));

	/// Wrap the command pool in our RAII wrapper
	this->commandPool = vulkan::VulkanCommandPoolHandle(commandPool, [this](VkCommandPool pool) {
		vkDestroyCommandPool(this->vulkanContext->getDevice()->getDevice(), pool, nullptr);
	});

	spdlog::info("Command pool {} created successfully", (long)this->commandPool.get());
}

void Renderer::createCommandBuffers() {
	/// We'll create one command buffer for each swap chain image
	uint32_t swapChainImageCount = this->vulkanContext->getSwapChain()->getSwapChainImages().size();
	this->commandBuffers.resize(swapChainImageCount);

	/// Set up command buffer allocation info
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = this->commandPool.get();

	/// Primary command buffers can be submitted to a queue for execution, but cannot be called from other command buffers
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	allocInfo.commandBufferCount = static_cast<uint32_t>(this->commandBuffers.size());

	/// Allocate the command buffers
	VK_CHECK(vkAllocateCommandBuffers(this->vulkanContext->getDevice()->getDevice(), &allocInfo, this->commandBuffers.data()));

	spdlog::info("Command buffers created successfully");
}

void Renderer::createRenderPass() {
	/// Color attachment description
	/// This describes how the color buffer will be used throughout the render pass
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = this->vulkanContext->getSwapChain()->getSwapChainImageFormat();
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; /// No multisampling
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; /// Clear the color buffer at the start of the render pass
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; /// Store the result for later use (e.g., presentation)
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; /// We're not using stencil buffer for color attachment
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; /// We don't care about the initial layout
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; /// The image will be presented in the swap chain

	/// Depth attachment description
	/// This describes how the depth buffer will be used throughout the render pass
	VkAttachmentDescription depthAttachment{};
	depthAttachment.format = this->depthBuffer->getFormat(); /// Use the format from our DepthBuffer class
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT; /// No multisampling for depth buffer
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; /// Clear the depth buffer at the start of the render pass
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; /// We don't need to store depth data after rendering
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; /// We're not using stencil buffer
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; /// We don't care about the initial layout
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; /// Optimal layout for depth attachment

	/// Attachment references
	/// These link the attachment descriptions to the actual attachments used in the subpass
	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0; /// Index of the color attachment in the attachment descriptions array
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; /// Layout to use during the subpass

	VkAttachmentReference depthAttachmentRef{};
	depthAttachmentRef.attachment = 1; /// Index of the depth attachment in the attachment descriptions array
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; /// Layout to use during the subpass

	/// Subpass description
	/// This describes the structure of a subpass within the render pass
	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; /// This is a graphics subpass
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef; /// Include depth attachment in the subpass

	/// Subpass dependencies
	/// These define the dependencies between subpasses or with external operations
	/// We need two dependencies: one for color and one for depth
	std::array<VkSubpassDependency, 2> dependencies;

	/// First dependency: Wait for color attachment output and depth testing before rendering
	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL; /// Dependency on operations outside the render pass
	dependencies[0].dstSubpass = 0; /// Our subpass index
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependencies[0].srcAccessMask = 0; /// No access in the source subpass
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = 0; /// Not needed, we're doing straightforward rendering without any special case

	/// Second dependency: Wait for rendering to finish before presenting
	dependencies[1].srcSubpass = 0; /// Our subpass index
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL; /// Dependency on operations outside the render pass
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[1].dstAccessMask = 0; /// No access in the destination subpass
	dependencies[1].dependencyFlags = 0; /// Not needed, we're doing straightforward rendering without any special case

	/// Combine attachments
	std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

	/// Render pass create info
	/// This aggregates all the information needed to create a render pass
	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
	renderPassInfo.pDependencies = dependencies.data();

	/// Create the render pass
	VkRenderPass renderPass;
	VK_CHECK(vkCreateRenderPass(this->vulkanContext->getDevice()->getDevice(), &renderPassInfo, nullptr, &renderPass));

	/// Wrap the render pass in our RAII wrapper for automatic resource management
	this->renderPass = vulkan::VulkanRenderPassHandle(renderPass, [this](VkRenderPass rp) {
		vkDestroyRenderPass(this->vulkanContext->getDevice()->getDevice(), rp, nullptr);
	});

	spdlog::info("Render pass with color and depth attachments created successfully");
}

void Renderer::createFramebuffers() {
	/// Framebuffers are the destination for the rendering operations
	/// We create one framebuffer for each image view in the swap chain

	/// Resize the framebuffer vector to match the number of swap chain images
	this->swapChainFramebuffers.resize(this->vulkanContext->getSwapChain()->getSwapChainImageViews().size());

	/// Iterate through each swap chain image view and create a framebuffer for it
	for (size_t i = 0; i < this->vulkanContext->getSwapChain()->getSwapChainImageViews().size(); i++) {
		/// We'll use two attachments for each framebuffer: color and depth
		/// The color attachment comes from the swap chain, while the depth attachment is shared
		std::array<VkImageView, 2> attachments = {
			this->vulkanContext->getSwapChain()->getSwapChainImageViews()[i].get(),
			this->depthBuffer->getImageView()
		};

		/// Create the framebuffer create info structure
		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = this->renderPass.get(); /// The render pass this framebuffer is compatible with
		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size()); /// Number of attachments (color and depth)
		framebufferInfo.pAttachments = attachments.data(); /// Pointer to the attachments array
		framebufferInfo.width = this->vulkanContext->getSwapChain()->getSwapChainExtent().width;
		framebufferInfo.height = this->vulkanContext->getSwapChain()->getSwapChainExtent().height;
		framebufferInfo.layers = 1; /// Number of layers in image arrays

		/// Create the framebuffer
		VkFramebuffer framebuffer;
		VK_CHECK(vkCreateFramebuffer(this->vulkanContext->getDevice()->getDevice(), &framebufferInfo, nullptr, &framebuffer));

		/// Store the framebuffer in our vector, wrapped in a VulkanFramebufferHandle for RAII
		this->swapChainFramebuffers[i] = vulkan::VulkanFramebufferHandle(framebuffer, [this](VkFramebuffer fb) {
			vkDestroyFramebuffer(this->vulkanContext->getDevice()->getDevice(), fb, nullptr);
		});
	}

	spdlog::info("Created {} framebuffers with color and depth attachments successfully", this->swapChainFramebuffers.size());
}

void Renderer::cleanupFramebuffers() {
	this->swapChainFramebuffers.clear();
	spdlog::info("Framebuffers cleaned up");
}

vulkan::VulkanShaderModuleHandle Renderer::createShaderModule(const std::vector<char>& code) {
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(this->vulkanContext->getDevice()->getDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		spdlog::error("Failed to create shader module");
		return vulkan::VulkanShaderModuleHandle();
	}

	return vulkan::VulkanShaderModuleHandle(shaderModule, [this](VkShaderModule sm) {
		vkDestroyShaderModule(this->vulkanContext->getDevice()->getDevice(), sm, nullptr);
	});
}

void Renderer::recordCommandBuffers() {
	/// Resize command buffers vector to match the number of framebuffers
	/// We need one command buffer for each swap chain image
	/// Start with clean command buffers
	/// First, free existing command buffers to prevent memory leaks
	vkFreeCommandBuffers(this->vulkanContext->getDevice()->getDevice(),
		this->commandPool.get(),
		static_cast<uint32_t>(this->commandBuffers.size()),
		this->commandBuffers.data());

	/// Resize for new recording
	/// One command buffer per framebuffer
	this->commandBuffers.resize(this->swapChainFramebuffers.size());

	/// Allocate new command buffers
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = this->commandPool.get();
	/// Primary command buffers can be submitted directly to queues
	/// Secondary command buffers can only be called from primary command buffers
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = static_cast<uint32_t>(this->commandBuffers.size());

	/// Allocate command buffers from the command pool
	VK_CHECK(vkAllocateCommandBuffers(this->vulkanContext->getDevice()->getDevice(),
		&allocInfo, this->commandBuffers.data()));

	/// Record commands for each framebuffer
	for (size_t i = 0; i < this->commandBuffers.size(); i++) {
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		VK_CHECK(vkBeginCommandBuffer(this->commandBuffers[i], &beginInfo));

		/// Set up render pass begin info
		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = this->renderPass.get();
		renderPassInfo.framebuffer = this->swapChainFramebuffers[i].get();
		/// Define the render area, typically the size of the framebuffer
		renderPassInfo.renderArea.offset = {0, 0};
		renderPassInfo.renderArea.extent = this->vulkanContext->getSwapChain()->getSwapChainExtent();

		/// Set clear values for color and depth attachments
		std::array<VkClearValue, 2> clearValues{};
		clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};  /// Black with 100% opacity
		/// For Reverse-Z, we clear to 0.0f instead of 1.0f
		/// This represents the furthest possible depth value in Reverse-Z
		/// Objects closer to the camera will have depth values closer to 1.0
		clearValues[1].depthStencil = {0.0f, 0};            /// Using 0.0f for Reverse-Z

		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();

		/// Begin the render pass
		/// VK_SUBPASS_CONTENTS_INLINE means the render pass commands will be embedded in the primary command buffer
		/// and no secondary command buffers will be executed
		vkCmdBeginRenderPass(this->commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		/// Collect render data from visible objects in the scene
		std::vector<Mesh::RenderData> renderData;
		this->scene->getRenderData(*this->camera, renderData);

		/// Track current material to minimize pipeline switches
		std::string currentMaterialName;

		/// Draw all visible objects
		for (const auto& data : renderData) {
			/// Get material name for pipeline lookup
			const auto& materialName = data.material->getName();

			/// Switch pipeline only if material changes
			if (materialName != currentMaterialName) {
				/// Get pipeline from PipelineManager using material name
				auto pipeline = this->pipelineManager->getPipeline(materialName);
				if (!pipeline) {
					spdlog::error("Failed to find pipeline for material '{}'", materialName);
					continue;
				}

				auto pipelineLayout = this->pipelineManager->getPipelineLayout(materialName);
				if (!pipelineLayout) {
					spdlog::error("Failed to find pipeline layout for material '{}'", materialName);
					continue;
				}

				/// Bind the new pipeline
				vkCmdBindPipeline(this->commandBuffers[i],
					VK_PIPELINE_BIND_POINT_GRAPHICS,
					pipeline->get());

				/// Set dynamic viewport and scissor
				/// These need to be set because we configured them as dynamic state
				VkViewport viewport{};
				viewport.x = 0.0f;
				viewport.y = 0.0f;
				viewport.width = static_cast<float>(this->vulkanContext->getSwapChain()->getSwapChainExtent().width);
				viewport.height = static_cast<float>(this->vulkanContext->getSwapChain()->getSwapChainExtent().height);
				viewport.minDepth = 0.0f;
				viewport.maxDepth = 1.0f;

				VkRect2D scissor{};
				scissor.offset = {0, 0};
				scissor.extent = this->vulkanContext->getSwapChain()->getSwapChainExtent();

				vkCmdSetViewport(this->commandBuffers[i], 0, 1, &viewport);
				vkCmdSetScissor(this->commandBuffers[i], 0, 1, &scissor);

				/// Bind camera and light descriptor sets (sets 0 and 1)
				std::array<VkDescriptorSet, 2> globalSets = {
					this->cameraDescriptorSets[i],
					this->lightDescriptorSets[i]
				};
				vkCmdBindDescriptorSets(
					this->commandBuffers[i],
					VK_PIPELINE_BIND_POINT_GRAPHICS,
					pipelineLayout->get(),
					0,  /// First set = 0 (camera)
					2,  /// Bind both global sets at once
					globalSets.data(),
					0, nullptr
				);

				currentMaterialName = materialName;
			}

			/// Bind material-specific resources
			data.material->bind(this->commandBuffers[i],
				this->pipelineManager->getPipelineLayout(materialName)->get());

			/// Update push constants with model matrix
			vkCmdPushConstants(
				this->commandBuffers[i],
				this->pipelineManager->getPipelineLayout(materialName)->get(),
				VK_SHADER_STAGE_VERTEX_BIT,
				0,
				sizeof(glm::mat4),
				&data.modelMatrix
			);

			/// Bind vertex and index buffers
			VkBuffer vertexBuffers[] = {data.vertexBuffer->get()};
			VkDeviceSize offsets[] = {0};
			vkCmdBindVertexBuffers(this->commandBuffers[i], 0, 1, vertexBuffers, offsets);
			vkCmdBindIndexBuffer(this->commandBuffers[i], data.indexBuffer->get(), 0,
				VK_INDEX_TYPE_UINT32);

			/// Draw the object
			vkCmdDrawIndexed(this->commandBuffers[i],
				data.indexBuffer->getIndexCount(),
				1, 0, 0, 0);
		}

		/// End the render pass
		vkCmdEndRenderPass(this->commandBuffers[i]);

		/// Finish recording the command buffer
		VK_CHECK(vkEndCommandBuffer(this->commandBuffers[i]));
	}

	// spdlog::debug("Command buffers recorded successfully");
}

void Renderer::createCameraUniformBuffer() {
	VkDeviceSize bufferSize = sizeof(CameraUBO);

	/// Create the uniform buffer
	/// Use VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	/// to ensure the buffer is accessible by the CPU and automatically synchronized
	this->vulkanBufferUtility->createBuffer(
		bufferSize,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		this->cameraBuffer,
		this->cameraBufferMemory
	);

	/// Initialize buffer with identity matrices
	CameraUBO initialData{};
	initialData.view = glm::mat4(1.0f);
	initialData.projection = glm::mat4(1.0f);

	/// Copy initial data to the buffer
	void* data;
	VK_CHECK(vkMapMemory(this->vulkanContext->getDevice()->getDevice(), this->cameraBufferMemory, 0, bufferSize, 0, &data));
	memcpy(data, &initialData, bufferSize);
	vkUnmapMemory(this->vulkanContext->getDevice()->getDevice(), this->cameraBufferMemory);

	spdlog::info("Camera uniform buffer created successfully");
}

void Renderer::updateCameraUniformBuffer() const {
	CameraUBO ubo{};

	/// Get the current view matrix from the camera
	ubo.view = this->camera->getViewMatrix();

	/// Calculate current aspect ratio from window dimensions
	float aspectRatio = static_cast<float>(this->width) /
		static_cast<float>(this->height);

	/// Get the projection matrix with current aspect ratio
	ubo.projection = this->camera->getProjectionMatrix(aspectRatio);

	/// Update GPU buffer with new camera data
	void* data;
	VK_CHECK(vkMapMemory(this->vulkanContext->getDevice()->getDevice(),
		this->cameraBufferMemory, 0, sizeof(ubo), 0, &data));
	memcpy(data, &ubo, sizeof(ubo));
	vkUnmapMemory(this->vulkanContext->getDevice()->getDevice(),
		this->cameraBufferMemory);
}


void Renderer::createDescriptorPool() {
	/// Define pool sizes for our different descriptor types
	/// Each type needs its own pool allocation
	std::array<VkDescriptorPoolSize, 2> poolSizes{};

	/// Camera buffer pool size
	/// One descriptor per swap chain image
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = static_cast<uint32_t>(this->swapChainFramebuffers.size());

	/// Light buffer pool size
	/// One descriptor per swap chain image
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[1].descriptorCount = static_cast<uint32_t>(this->swapChainFramebuffers.size());

	/// Create the descriptor pool
	/// We need enough space for both camera and light descriptors per frame
	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	/// Multiply maxSets by 2 because we need two sets (camera + light) per frame
	poolInfo.maxSets = static_cast<uint32_t>(this->swapChainFramebuffers.size() * 2);

	VK_CHECK(vkCreateDescriptorPool(
		this->vulkanContext->getDevice()->getDevice(),
		&poolInfo,
		nullptr,
		&this->descriptorPool));

	spdlog::info("Created descriptor pool for camera and light descriptors");
}

void Renderer::createDescriptorSets() {
	/// Calculate number of descriptor sets needed
	/// One set per swap chain image for both camera and lighting data
	size_t numFrames = this->swapChainFramebuffers.size();

	/// Create storage for camera descriptor sets
	this->cameraDescriptorSets.resize(numFrames);
	/// Create storage for light descriptor sets
	this->lightDescriptorSets.resize(numFrames);

	/// First, allocate camera descriptor sets
	{
		/// Create layouts array using global layout from pipeline manager
		std::vector<VkDescriptorSetLayout> cameraLayouts(numFrames,
			this->pipelineManager->getCameraDescriptorLayout());

		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = this->descriptorPool;
		allocInfo.descriptorSetCount = static_cast<uint32_t>(numFrames);
		allocInfo.pSetLayouts = cameraLayouts.data();

		VK_CHECK(vkAllocateDescriptorSets(
			this->vulkanContext->getDevice()->getDevice(),
			&allocInfo,
			this->cameraDescriptorSets.data()));
	}

	/// Then, allocate light descriptor sets
	{
		/// Create layouts array using global layout from pipeline manager
		std::vector<VkDescriptorSetLayout> lightLayouts(numFrames,
			this->pipelineManager->getLightDescriptorLayout());

		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = this->descriptorPool;
		allocInfo.descriptorSetCount = static_cast<uint32_t>(numFrames);
		allocInfo.pSetLayouts = lightLayouts.data();

		VK_CHECK(vkAllocateDescriptorSets(
			this->vulkanContext->getDevice()->getDevice(),
			&allocInfo,
			this->lightDescriptorSets.data()));
	}

	/// Update descriptors for each frame
	for (size_t i = 0; i < numFrames; i++) {
		/// Update camera descriptor
		{
			VkDescriptorBufferInfo bufferInfo{};
			bufferInfo.buffer = this->cameraBuffer.get();
			bufferInfo.offset = 0;
			bufferInfo.range = sizeof(CameraUBO);

			VkWriteDescriptorSet descriptorWrite{};
			descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrite.dstSet = this->cameraDescriptorSets[i];
			descriptorWrite.dstBinding = 0;
			descriptorWrite.dstArrayElement = 0;
			descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptorWrite.descriptorCount = 1;
			descriptorWrite.pBufferInfo = &bufferInfo;

			vkUpdateDescriptorSets(
				this->vulkanContext->getDevice()->getDevice(),
				1,
				&descriptorWrite,
				0, nullptr);
		}

		/// Update light descriptor
		{
			VkDescriptorBufferInfo bufferInfo{};
			bufferInfo.buffer = this->lightBuffer.get();
			bufferInfo.offset = 0;
			bufferInfo.range = sizeof(LightData) * LightManager::MaxLights;

			VkWriteDescriptorSet descriptorWrite{};
			descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrite.dstSet = this->lightDescriptorSets[i];
			descriptorWrite.dstBinding = 0;
			descriptorWrite.dstArrayElement = 0;
			descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptorWrite.descriptorCount = 1;
			descriptorWrite.pBufferInfo = &bufferInfo;

			vkUpdateDescriptorSets(
				this->vulkanContext->getDevice()->getDevice(),
				1,
				&descriptorWrite,
				0, nullptr);
		}
	}

	spdlog::info("Created and updated descriptor sets for {} frames", numFrames);
}

void Renderer::handleCameraInput(SDL_Window* window, const SDL_Event& event) const {
	/// Delegate input handling to the camera
	/// This keeps the camera logic encapsulated within the EditorCamera class
	this->camera->handleInput(window, event);
}

void Renderer::createSyncObjects() {
	/// Create semaphores and fence for frame synchronization
	/// Semaphores are used to coordinate operations within the GPU command queue
	/// Fences are used to synchronize the CPU with the GPU

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	/// Create the fence in a signaled state so that the first frame doesn't wait indefinitely
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	/// Create semaphores and fence
	VK_CHECK(vkCreateSemaphore(this->vulkanContext->getDevice()->getDevice(), &semaphoreInfo, nullptr, &this->imageAvailableSemaphore));
	VK_CHECK(vkCreateSemaphore(this->vulkanContext->getDevice()->getDevice(), &semaphoreInfo, nullptr, &this->renderFinishedSemaphore));
	VK_CHECK(vkCreateFence(this->vulkanContext->getDevice()->getDevice(), &fenceInfo, nullptr, &this->inFlightFence));

	spdlog::info("Synchronization objects created successfully");
}

void Renderer::cleanupSyncObjects() {
	/// Clean up synchronization objects
	/// This should be called during the Renderer's cleanup process

	vkDestroySemaphore(this->vulkanContext->getDevice()->getDevice(), this->renderFinishedSemaphore, nullptr);
	vkDestroySemaphore(this->vulkanContext->getDevice()->getDevice(), this->imageAvailableSemaphore, nullptr);
	vkDestroyFence(this->vulkanContext->getDevice()->getDevice(), this->inFlightFence, nullptr);

	spdlog::info("Synchronization objects cleaned up");
}

void Renderer::initializeDepthBuffer() {
	/// We create the depth buffer after the swap chain is initialized
	/// This ensures we have the correct dimensions for the depth buffer

	/// Check if the Vulkan context and swap chain are initialized
	if (!this->vulkanContext || !this->vulkanContext->getSwapChain()) {
		throw vulkan::VulkanException(VK_ERROR_INITIALIZATION_FAILED,
			"Attempted to initialize depth buffer before Vulkan context or swap chain",
			__FUNCTION__, __FILE__, __LINE__);
	}

	/// Get the swap chain extent for depth buffer dimensions
	VkExtent2D swapChainExtent = this->vulkanContext->getSwapChain()->getSwapChainExtent();

	/// Create the depth buffer
	/// We use a unique_ptr for automatic memory management
	this->depthBuffer = std::make_unique<vulkan::DepthBuffer>(
		this->vulkanContext->getDevice()->getDevice(),
		this->vulkanContext->getPhysicalDevice()
	);

	/// Initialize the depth buffer with the swap chain dimensions
	/// This ensures the depth buffer matches the size of our render targets
	this->depthBuffer->initialize(swapChainExtent.width, swapChainExtent.height);

	spdlog::info("Depth buffer initialized successfully");
}

void Renderer::initializeScene() {
	/// Create main directional light (sun)
	auto sunLight = std::make_shared<DirectionalLight>(glm::vec3(1.0f, 1.0f, 1.0f));
	sunLight->setColor(glm::vec3(1.0f, 0.95f, 0.8f));  /// Warm sunlight
	sunLight->setIntensity(1.0f);
	sunLight->setAmbient(glm::vec3(0.1f, 0.1f, 0.15f));
	this->lightManager->addLight(sunLight);

	/// Create blue fill light from the left
	auto fillLight = std::make_shared<DirectionalLight>(glm::vec3(1.0f, -0.5f, 0.0f));
	fillLight->setColor(glm::vec3(0.3f, 0.4f, 0.8f));  /// Cool blue color
	fillLight->setIntensity(0.5f);                      /// Less intense than sun
	fillLight->setAmbient(glm::vec3(0.0f));            /// No ambient contribution
	this->lightManager->addLight(fillLight);

	/// Create red rim light from behind
	auto rimLight = std::make_shared<DirectionalLight>(glm::vec3(0.0f, 0.0f, 1.0f));
	rimLight->setColor(glm::vec3(0.8f, 0.3f, 0.2f));   /// Warm red color
	rimLight->setIntensity(0.3f);                       /// Subtle intensity
	rimLight->setAmbient(glm::vec3(0.0f));             /// No ambient contribution
	this->lightManager->addLight(rimLight);

	/// Get the default material for our test objects
	auto defaultMaterial = this->materialManager->getMaterial("default");
	if (!defaultMaterial) {
		throw vulkan::VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Default material not found",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	/// Create a simple cube in the scene for initial testing
	/// We use the Scene API to create and position objects
	auto rootNode = this->scene->getRoot();

	/// Create a metallic material for the icosphere
	/// We use different material properties to better showcase the geometry
	auto metallicMaterial = this->materialManager->createPBRMaterial("metallic");
	metallicMaterial->setBaseColor(glm::vec4(0.95f, 0.95f, 0.95f, 1.0f));  /// Almost white
	metallicMaterial->setMetallic(1.0f);     /// Fully metallic
	metallicMaterial->setRoughness(0.2f);    /// Fairly smooth for good reflection
	metallicMaterial->setAmbient(1.0f);      /// Full ambient occlusion

	auto wireframeMaterial = this->materialManager->createWireframeMaterial("wireframe");

	/// Add an icosphere to demonstrate spherical geometry
	/// We place it at the center where it's easy to observe
	auto icosphereNode = this->scene->createNode("TestIcosphere", rootNode);
	auto icosphereMesh = this->meshManager->createMesh<IcosphereMesh>(1.0f, 2);
	icosphereMesh->setMaterial(wireframeMaterial);
	icosphereNode->setMesh(std::move(icosphereMesh));

	/// Position a large icosphere in the middle of all cubes
	/// It intersects some of the cude to see, that depth rendering works correctly
	/// This makes it easier to see its relationship to other objects
	scene::Transform icosphereTransform;
	icosphereTransform.position = glm::vec3(0.0f, 0.0f, 0.0f);
	icosphereTransform.scale = glm::vec3(2.9f);
	icosphereNode->setLocalTransform(icosphereTransform);

	/// Create a node for our test cube
	auto cubeNode = this->scene->createNode("TestCube", rootNode);

	/// Create and set up the cube mesh using MeshManager
	auto cubeMesh = this->meshManager->createMesh<CubeMesh>();

	/// Set the material before adding to scene
	cubeMesh->setMaterial(defaultMaterial);
	cubeNode->setMesh(std::move(cubeMesh));

	/// Position the cube slightly offset from center
	scene::Transform transform;
	transform.position = glm::vec3(-1.0f, -1.0f, -1.0f);
	cubeNode->setLocalTransform(transform);

	/// Create a second cube for testing hierarchical transforms
	auto cubeNode2 = this->scene->createNode("TestCube2", rootNode);
	auto cubeMesh2 = this->meshManager->createMesh<CubeMesh>();

	/// Set the material before adding to scene
	cubeMesh2->setMaterial(defaultMaterial);
	cubeNode2->setMesh(std::move(cubeMesh2));

	/// Position the second cube offset from the first
	scene::Transform transform2;
	transform2.position = glm::vec3(1.5f, 1.5f, 1.5f);
	cubeNode2->setLocalTransform(transform2);

	/// For the grid of cubes, create some varied materials
	auto redMaterial = this->materialManager->createPBRMaterial("red");
	redMaterial->setBaseColor(glm::vec4(1.0f, 0.2f, 0.2f, 1.0f));
	redMaterial->setRoughness(0.7f);

	auto blueMaterial = this->materialManager->createPBRMaterial("blue");
	blueMaterial->setBaseColor(glm::vec4(0.2f, 0.2f, 1.0f, 1.0f));
	blueMaterial->setMetallic(0.8f);

	/// Add more cubes to better show lighting
	/// Create a grid of cubes to demonstrate lighting from different angles
	for (int x = -2; x <= 2; x++) {
		for (int z = -2; z <= 2; z++) {
			auto cubeNode = this->scene->createNode(
				"GridCube_" + std::to_string(x) + "_" + std::to_string(z),
				this->scene->getRoot()
			);

			auto cubeMesh = this->meshManager->createMesh<CubeMesh>();

			/// Assign different materials based on position
			/// This creates a checkerboard pattern of materials
			if ((x + z) % 2 == 0) {
				cubeMesh->setMaterial(redMaterial);
			} else {
				cubeMesh->setMaterial(blueMaterial);
			}

			cubeNode->setMesh(std::move(cubeMesh));

			scene::Transform transform;
			transform.position = glm::vec3(x * 2.0f, 0.0f, z * 2.0f);
			transform.scale = glm::vec3(0.5f);
			cubeNode->setLocalTransform(transform);
		}
	}

	/// Update bounds after creating all objects
	rootNode->updateBoundsIfNeeded();

	spdlog::info("Scene initialized with test objects");
}

void Renderer::createLightUniformBuffer() {
	/// Calculate required buffer size
	/// We allocate space for the maximum number of lights
	/// This simplifies buffer management as size remains constant
	VkDeviceSize bufferSize = sizeof(LightData) * LightManager::MaxLights;

	/// Create the uniform buffer
	/// We use VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT to allow CPU updates
	/// and VK_MEMORY_PROPERTY_HOST_COHERENT_BIT to ensure synchronization
	this->vulkanBufferUtility->createBuffer(
		bufferSize,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		this->lightBuffer,
		this->lightBufferMemory
	);

	/// Initialize buffer with empty light data
	/// This ensures valid data even before any lights are added
	std::vector<LightData> initialData(LightManager::MaxLights);
	void* data;
	VK_CHECK(vkMapMemory(this->vulkanContext->getDevice()->getDevice(),
		this->lightBufferMemory, 0, bufferSize, 0, &data));
	memcpy(data, initialData.data(), bufferSize);
	vkUnmapMemory(this->vulkanContext->getDevice()->getDevice(),
		this->lightBufferMemory);

	spdlog::info("Light uniform buffer created with size {} bytes", bufferSize);
}

/// Update the light uniform buffer with current light data
/// This should be called when lights change or every frame if lights are dynamic
void Renderer::updateLightUniformBuffer() const {
	/// Get current light data from the manager
	/// This data is already in GPU-friendly format
	auto lightData = this->lightManager->getLightData();

	/// Calculate buffer size
	VkDeviceSize bufferSize = sizeof(LightData) * LightManager::MaxLights;

	/// Map the buffer memory and update it with new data
	void* data;
	VK_CHECK(vkMapMemory(this->vulkanContext->getDevice()->getDevice(),
		this->lightBufferMemory, 0, bufferSize, 0, &data));
	memcpy(data, lightData.data(), bufferSize);
	vkUnmapMemory(this->vulkanContext->getDevice()->getDevice(),
		this->lightBufferMemory);

	spdlog::trace("Updated light uniform buffer with {} lights",
		this->lightManager->getLightCount());
}

void Renderer::initializeMaterials() {
	/// Create material manager
	/// We pass the Vulkan device handles needed for resource creation
	this->materialManager = std::make_unique<MaterialManager>(
		this->vulkanContext->getDevice()->getDevice(),
		this->vulkanContext->getPhysicalDevice()
	);

	/// Create default PBR material
	/// This provides our standard material for basic objects
	auto defaultMaterial = this->materialManager->createPBRMaterial("default");
	defaultMaterial->setBaseColor(glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
	defaultMaterial->setRoughness(0.5f);
	defaultMaterial->setMetallic(0.0f);

	/// Create pipeline for default material
	/// This ensures the pipeline is ready when we start rendering
	auto defaultPipeline = this->pipelineManager->createPipeline(*defaultMaterial);
	if (!defaultPipeline) {
		throw vulkan::VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Failed to create pipeline for default material",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	/// Create a metallic material for the icosphere
	/// We use different material properties to better showcase the geometry
	auto metallicMaterial = this->materialManager->createPBRMaterial("metallic");
	metallicMaterial->setBaseColor(glm::vec4(0.95f, 0.95f, 0.95f, 1.0f));
	metallicMaterial->setMetallic(1.0f);     /// Fully metallic
	metallicMaterial->setRoughness(0.2f);    /// Fairly smooth for good reflection
	metallicMaterial->setAmbient(1.0f);      /// Full ambient occlusion

	/// Create pipeline for metallic material
	auto metallicPipeline = this->pipelineManager->createPipeline(*metallicMaterial);
	if (!metallicPipeline) {
		throw vulkan::VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Failed to create pipeline for metallic material",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	/// For the grid of cubes, create some varied materials
	auto redMaterial = this->materialManager->createPBRMaterial("red");
	redMaterial->setBaseColor(glm::vec4(1.0f, 0.2f, 0.2f, 1.0f));
	redMaterial->setRoughness(0.7f);

	/// Create pipeline for red material
	auto redPipeline = this->pipelineManager->createPipeline(*redMaterial);
	if (!redPipeline) {
		throw vulkan::VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Failed to create pipeline for red material",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	auto blueMaterial = this->materialManager->createPBRMaterial("blue");
	blueMaterial->setBaseColor(glm::vec4(0.2f, 0.2f, 1.0f, 1.0f));
	blueMaterial->setMetallic(0.8f);

	/// Create pipeline for blue material
	auto bluePipeline = this->pipelineManager->createPipeline(*blueMaterial);
	if (!bluePipeline) {
		throw vulkan::VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Failed to create pipeline for blue material",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	auto wireframeMaterial = this->materialManager->createWireframeMaterial("wireframe");
	wireframeMaterial->setColor(glm::vec3(1.0f, 0.0f, 0.0f));

	/// Create pipeline for wireframe material
	auto wireframePipeline = this->pipelineManager->createPipeline(*wireframeMaterial);
	if (!wireframePipeline) {
		throw vulkan::VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Failed to create pipeline for wireframe material",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	spdlog::info("Materials and pipelines initialized successfully");
}

}
