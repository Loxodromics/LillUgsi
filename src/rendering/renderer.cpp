#include "renderer.h"
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
	:  vulkanContext(std::make_unique<vulkan::VulkanContext>())
	, width(0)
	, height(0)
	, isCleanedUp(false) {
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
		SDL_GetWindowSizeInPixels(window, reinterpret_cast<int*>(&this->width), reinterpret_cast<int*>(&this->height));

		/// Create render pass
		this->createRenderPass();

		/// Create framebuffers
		this->createFramebuffers();

		/// Create command pool
		this->createCommandPool();

		/// Initialize geometry
		this->initializeGeometry();

		/// Create Vulkan buffer utility
		this->vulkanBuffer = std::make_unique<vulkan::VulkanBuffer>(
			this->vulkanContext->getDevice()->getDevice(),
			this->vulkanContext->getPhysicalDevice()
		);

		/// Create vertex buffer
		this->createVertexBuffer();

		/// Create index buffer
		this->createIndexBuffer();

		/// Create camera uniform buffer
		this->createCameraUniformBuffer();

		/// Create descriptor set layout
		this->createDescriptorSetLayout();

		/// Create descriptor pool
		this->createDescriptorPool();

		/// Create descriptor sets
		this->createDescriptorSets();

		/// Create graphics pipeline
		this->createGraphicsPipeline();

		/// Create command buffers
		this->createCommandBuffers();

		/// Record command buffers
		this->recordCommandBuffers();

		/// Create synchronization objects
		this->createSyncObjects();

		/// Initialize camera
		this->camera = std::make_unique<EditorCamera>(glm::vec3(0.0f, 2.0f, 5.0f));

		/// Set up the camera's aspect ratio based on the window size
		/// This ensures the initial view is correct
		float aspectRatio = static_cast<float>(this->width) / static_cast<float>(this->height);
		this->camera->getProjectionMatrix(aspectRatio);

		spdlog::info("Renderer initialized successfully");
		return true;
	}
	catch (const vulkan::VulkanException& e) {
		spdlog::error("Vulkan error during renderer initialization: {}", e.what());
		return false;
	}
	catch (const std::exception& e) {
		spdlog::error("Error during renderer initialization: {}", e.what());
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
	if (this->descriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(this->vulkanContext->getDevice()->getDevice(), this->descriptorSetLayout, nullptr);
		this->descriptorSetLayout = VK_NULL_HANDLE;
	}

	/// Clean up uniform buffers
	if (this->cameraBuffer) {
		this->cameraBuffer.reset();
	}
	if (this->cameraBufferMemory != VK_NULL_HANDLE) {
		vkFreeMemory(this->vulkanContext->getDevice()->getDevice(), this->cameraBufferMemory, nullptr);
		this->cameraBufferMemory = VK_NULL_HANDLE;
	}

	/// Clean up vertex and index buffers
	if (this->vertexBuffer) {
		this->vertexBuffer.reset();
	}
	if (this->vertexBufferMemory != VK_NULL_HANDLE) {
		vkFreeMemory(this->vulkanContext->getDevice()->getDevice(), this->vertexBufferMemory, nullptr);
		this->vertexBufferMemory = VK_NULL_HANDLE;
	}
	if (this->indexBuffer) {
		this->indexBuffer.reset();
	}
	if (this->indexBufferMemory != VK_NULL_HANDLE) {
		vkFreeMemory(this->vulkanContext->getDevice()->getDevice(), this->indexBufferMemory, nullptr);
		this->indexBufferMemory = VK_NULL_HANDLE;
	}

	this->vulkanBuffer.reset();

	/// Clean up framebuffers
	this->cleanupFramebuffers();

	/// Clean up render pass
	this->renderPass.reset();

	/// Reset command pool
	this->commandPool.reset();

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

		spdlog::info("Swap chain, framebuffers, and command buffers recreated successfully");
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

	spdlog::info("Command pool created successfully");
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
	/// VkAttachmentDescription: Describes a framebuffer attachment (e.g., color, depth, or stencil buffer).
/// It defines how the attachment will be used throughout the render pass.
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = this->vulkanContext->getSwapChain()->getSwapChainImageFormat();
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
	VK_CHECK(vkCreateRenderPass(this->vulkanContext->getDevice()->getDevice(), &renderPassInfo, nullptr, &renderPass));

	/// Wrap the render pass in our RAII wrapper for automatic resource management
	this->renderPass = vulkan::VulkanRenderPassHandle(renderPass, [this](VkRenderPass rp) {
		vkDestroyRenderPass(this->vulkanContext->getDevice()->getDevice(), rp, nullptr);
	});

	spdlog::info("Render pass created successfully");
}

void Renderer::createFramebuffers() {
	/// Framebuffers are the destination for the rendering operations.
/// We create one framebuffer for each image view in the swap chain.

	/// Resize the framebuffer vector to match the number of swap chain images
	this->swapChainFramebuffers.resize(this->vulkanContext->getSwapChain()->getSwapChainImageViews().size());

	/// Iterate through each swap chain image view and create a framebuffer for it
	for (size_t i = 0; i < this->vulkanContext->getSwapChain()->getSwapChainImageViews().size(); i++) {
		/// We'll use only one attachment for now - the color attachment
		VkImageView attachments[] = {
			this->vulkanContext->getSwapChain()->getSwapChainImageViews()[i].get()
		};

		/// Create the framebuffer create info structure
		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = this->renderPass.get(); /// The render pass this framebuffer is compatible with
		framebufferInfo.attachmentCount = 1; /// Number of attachments (just color for now)
		framebufferInfo.pAttachments = attachments; /// Pointer to the attachments array
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

	spdlog::info("Created {} framebuffers successfully", this->swapChainFramebuffers.size());
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

void Renderer::createGraphicsPipeline()
{
	/// Initialize the PipelineManager
	/// This manager will handle the creation and management of our graphics pipelines
	this->pipelineManager = std::make_unique<vulkan::PipelineManager>(
		this->vulkanContext->getDevice()->getDevice(),
		this->renderPass.get()
	);

	/// Define the vertex input binding
	/// This describes how to interpret the vertex data that will be input to the vertex shader
	VkVertexInputBindingDescription bindingDescription{};
	bindingDescription.binding = 0; // We're using a single vertex buffer, so we use binding 0
	bindingDescription.stride = sizeof(Vertex); // Size of each vertex
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // Move to the next data entry after each vertex

	/// Define the vertex attribute descriptions
	/// These describe how to extract vertex attributes from the vertex buffer data
	std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

	/// Position attribute
	attributeDescriptions[0].binding = 0; // Which binding the per-vertex data comes from
	attributeDescriptions[0].location = 0; // Location in the vertex shader
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT; // Format of the attribute (vec3)
	attributeDescriptions[0].offset = offsetof(Vertex, pos); // Offset of the attribute in the vertex struct

	/// Color attribute
	attributeDescriptions[1].binding = 0;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[1].offset = offsetof(Vertex, color);

	/// Create the graphics pipeline using the PipelineManager
	/// This encapsulates all the complex pipeline creation logic in the PipelineManager
	this->graphicsPipeline = this->pipelineManager->createGraphicsPipeline(
		"mainPipeline",
		"shaders/vert.spv",
		"shaders/frag.spv",
		bindingDescription,
		std::vector<VkVertexInputAttributeDescription>(attributeDescriptions.begin(), attributeDescriptions.end()),
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		this->vulkanContext->getSwapChain()->getSwapChainExtent().width,
		this->vulkanContext->getSwapChain()->getSwapChainExtent().height,
		this->descriptorSetLayout  /// Pass the descriptor set layout
	);

	/// Retrieve the pipeline layout
	this->pipelineLayout = this->pipelineManager->getPipelineLayout("mainPipeline");

	if (!this->graphicsPipeline || !this->pipelineLayout) {
		throw vulkan::VulkanException(VK_ERROR_INITIALIZATION_FAILED, "Failed to create graphics pipeline or retrieve pipeline layout", __FUNCTION__, __FILE__, __LINE__);
	}
	
	/// At this point, we have successfully created a graphics pipeline and retrieved its layout
	/// The pipeline is ready to be used for rendering
	/// The pipeline layout can be used when binding descriptor sets or push constants
	spdlog::info("Graphics pipeline and layout created successfully");
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
	VK_CHECK(vkAllocateCommandBuffers(this->vulkanContext->getDevice()->getDevice(), &allocInfo, this->commandBuffers.data()));

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
		renderPassInfo.renderArea.extent = this->vulkanContext->getSwapChain()->getSwapChainExtent();

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
		vkCmdBindPipeline(this->commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, this->graphicsPipeline->get());

		/// Bind the descriptor set for this frame
		vkCmdBindDescriptorSets(
			this->commandBuffers[i],
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			this->pipelineLayout->get(),
			0,
			1,
			&this->descriptorSets[i],
			0,
			nullptr
		);

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

	/// Create the uniform buffer
	/// Use VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	/// to ensure the buffer is accessible by the CPU and automatically synchronized
	this->vulkanBuffer->createBuffer(
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

void Renderer::updateCameraUniformBuffer() {
	/// Update the camera's state
	/// This should be called each frame to ensure smooth camera movement
	this->camera->update(0.016f);  // Assuming 60 FPS, you might want to use actual delta time

	CameraUBO ubo{};
	ubo.view = this->camera->getViewMatrix();
	ubo.projection =
			this->camera->getProjectionMatrix(this->vulkanContext->getSwapChain()->getSwapChainExtent().width /
											  static_cast<float>(this->vulkanContext->getSwapChain()->getSwapChainExtent().height));

	/// Copy the new data to the uniform buffer
	void* data;
	VK_CHECK(vkMapMemory(this->vulkanContext->getDevice()->getDevice(), this->cameraBufferMemory, 0, sizeof(ubo), 0, &data));
	memcpy(data, &ubo, sizeof(ubo));
	vkUnmapMemory(this->vulkanContext->getDevice()->getDevice(), this->cameraBufferMemory);
}

void Renderer::createVertexBuffer() {
	/// Calculate the size of the buffer we need
	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

	/// We use a staging buffer for several reasons:
	/// 1. It allows us to use a memory type that is host visible (CPU can write to it)
	/// 2. We can then transfer this to a device local memory, which is faster for the GPU to read from
	/// This two-step process is often faster than using a buffer that is both host visible and device local,
	/// especially on discrete GPUs where device local memory is separate from system memory
	vulkan::VulkanBufferHandle stagingBuffer;
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
	VK_CHECK(vkMapMemory(this->vulkanContext->getDevice()->getDevice(), stagingBufferMemory, 0, bufferSize, 0, &data));

	/// Copy our vertex data to the mapped memory
	/// memcpy is used here for simplicity, but for larger data sets or more complex scenarios,
	/// we might want to consider more sophisticated methods of populating our vertex buffer
	memcpy(data, vertices.data(), (size_t) bufferSize);

	/// Unmap the memory
	/// We don't need to call vkFlushMappedMemoryRanges because we used a coherent memory type
	vkUnmapMemory(this->vulkanContext->getDevice()->getDevice(), stagingBufferMemory);

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
		this->vulkanContext->getDevice()->getGraphicsQueue(),
		stagingBuffer.get(),
		this->vertexBuffer.get(),
		bufferSize
	);

	/// Clean up the staging buffer and its memory
	/// It's important to do this explicitly to ensure proper resource management
	/// The staging buffer was only needed to transfer the data to the device local vertex buffer
	stagingBuffer.reset();
	vkFreeMemory(this->vulkanContext->getDevice()->getDevice(), stagingBufferMemory, nullptr);

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
	vulkan::VulkanBufferHandle stagingBuffer;
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
	VK_CHECK(vkMapMemory(this->vulkanContext->getDevice()->getDevice(), stagingBufferMemory, 0, bufferSize, 0, &data));

	/// Copy our index data to the mapped memory
	/// For larger datasets, you might want to consider using a more sophisticated
	/// method of populating your buffer
	memcpy(data, indices.data(), (size_t) bufferSize);

	/// Unmap the memory
	/// No need to manually flush due to the coherent memory type
	vkUnmapMemory(this->vulkanContext->getDevice()->getDevice(), stagingBufferMemory);

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
		this->vulkanContext->getDevice()->getGraphicsQueue(),
		stagingBuffer.get(),
		this->indexBuffer.get(),
		bufferSize
	);

	/// Clean up the staging buffer and its memory
	/// This step is crucial for proper resource management
	/// The staging buffer has served its purpose and is no longer needed
	stagingBuffer.reset();
	vkFreeMemory(this->vulkanContext->getDevice()->getDevice(), stagingBufferMemory, nullptr);

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

void Renderer::createDescriptorSetLayout() {
	/// Define how the uniform buffer will be accessed in the shader
	VkDescriptorSetLayoutBinding uboLayoutBinding{};
	uboLayoutBinding.binding = 0; /// Corresponds to the binding in the shader
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1; /// We only have one uniform buffer
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; /// This uniform buffer is used in the vertex shader
	uboLayoutBinding.pImmutableSamplers = nullptr; /// Only relevant for image sampling, which we're not doing here

	/// Create the descriptor set layout
	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1; /// We only have one binding
	layoutInfo.pBindings = &uboLayoutBinding;

	/// Create the descriptor set layout
	/// This defines the interface between the shader and the uniform buffer
	VK_CHECK(vkCreateDescriptorSetLayout(this->vulkanContext->getDevice()->getDevice(), &layoutInfo, nullptr, &this->descriptorSetLayout));

	spdlog::info("Descriptor set layout created successfully");
}

void Renderer::createDescriptorPool() {
	/// Define the types of descriptors we'll be allocating
	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSize.descriptorCount = static_cast<uint32_t>(this->swapChainFramebuffers.size()); /// One uniform buffer per frame in flight

	/// Create the descriptor pool
	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 1; /// We only have one type of descriptor
	poolInfo.pPoolSizes = &poolSize;
	/// Set the maximum number of descriptor sets that can be allocated
	poolInfo.maxSets = static_cast<uint32_t>(this->swapChainFramebuffers.size());

	/// Create the descriptor pool
	/// This pool will be used to allocate the descriptor sets
	VK_CHECK(vkCreateDescriptorPool(this->vulkanContext->getDevice()->getDevice(), &poolInfo, nullptr, &this->descriptorPool));

	spdlog::info("Descriptor pool created successfully");
}

void Renderer::createDescriptorSets() {
	/// Prepare to allocate one descriptor set for each frame in flight
	std::vector<VkDescriptorSetLayout> layouts(this->swapChainFramebuffers.size(), this->descriptorSetLayout);
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = this->descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(this->swapChainFramebuffers.size());
	allocInfo.pSetLayouts = layouts.data();

	/// Allocate the descriptor sets
	this->descriptorSets.resize(this->swapChainFramebuffers.size());
	VK_CHECK(vkAllocateDescriptorSets(this->vulkanContext->getDevice()->getDevice(), &allocInfo, this->descriptorSets.data()));

	/// Update each descriptor set with the uniform buffer info
	for (size_t i = 0; i < this->swapChainFramebuffers.size(); i++) {
		/// Describe the uniform buffer to Vulkan
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = this->cameraBuffer.get();
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(CameraUBO);

		/// Prepare to write the descriptor set
		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = this->descriptorSets[i];
		descriptorWrite.dstBinding = 0; /// Corresponds to the binding in the shader
		descriptorWrite.dstArrayElement = 0; /// We're not using an array of descriptors
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pBufferInfo = &bufferInfo;

		/// Update the descriptor set
		vkUpdateDescriptorSets(this->vulkanContext->getDevice()->getDevice(), 1, &descriptorWrite, 0, nullptr);
	}

	spdlog::info("Descriptor sets created and updated successfully");
}

void Renderer::handleCameraInput(SDL_Window* window, const SDL_Event& event) {
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

}
