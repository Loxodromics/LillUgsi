#include "framebuffermanager.h"
#include <spdlog/spdlog.h>

namespace lillugsi::vulkan {

FramebufferManager::FramebufferManager(VkDevice device)
	: device(device)
	, initialized(false) {
	spdlog::debug("Creating framebuffer manager");
}

FramebufferManager::~FramebufferManager() {
	/// Ensure resources are cleaned up when the manager is destroyed
	/// This prevents resource leaks if the user forgets to call cleanup
	if (this->initialized) {
		this->cleanup();
	}
}

bool FramebufferManager::initialize() {
	/// Nothing to initialize yet, but this provides a hook for future extensions
	/// Having an initialize method maintains consistency with other manager classes
	this->initialized = true;
	spdlog::info("Framebuffer manager initialized");
	return true;
}

void FramebufferManager::cleanup() {
	if (!this->initialized) {
		spdlog::warn("Attempting to clean up uninitialized framebuffer manager");
		return;
	}

	/// Clear the vector of framebuffer handles
	/// This triggers the destruction of all framebuffers through RAII
	size_t count = this->swapChainFramebuffers.size();
	this->swapChainFramebuffers.clear();

	spdlog::info("Cleaned up {} framebuffers", count);
	this->initialized = false;
}

void FramebufferManager::createSwapChainFramebuffers(
	VkRenderPass renderPass,
	const std::vector<VulkanImageViewHandle>& swapChainImageViews,
	VkImageView depthImageView,
	uint32_t width,
	uint32_t height) {

	/// Validate input parameters
	if (!renderPass) {
		throw VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Cannot create framebuffers with null render pass",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	if (swapChainImageViews.empty()) {
		throw VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Cannot create framebuffers with empty image views",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	if (!depthImageView) {
		throw VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Cannot create framebuffers with null depth image view",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	/// Clean up any existing framebuffers first
	/// This ensures we don't leak resources when recreating framebuffers
	if (!this->swapChainFramebuffers.empty()) {
		this->swapChainFramebuffers.clear();
	}

	/// Resize the framebuffer vector to match the number of swap chain images
	/// Each swap chain image needs its own framebuffer
	this->swapChainFramebuffers.resize(swapChainImageViews.size());

	/// Iterate through each swap chain image view and create a framebuffer for it
	for (size_t i = 0; i < swapChainImageViews.size(); i++) {
		/// We'll use two attachments for each framebuffer: color and depth
		/// The color attachment comes from the swap chain, while the depth attachment is shared
		std::array<VkImageView, 2> attachments = {
			swapChainImageViews[i].get(),
			depthImageView
		};

		/// Create the framebuffer create info structure
		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass; /// The render pass this framebuffer is compatible with
		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size()); /// Number of attachments (color and depth)
		framebufferInfo.pAttachments = attachments.data(); /// Pointer to the attachments array
		framebufferInfo.width = width;
		framebufferInfo.height = height;
		framebufferInfo.layers = 1; /// Number of layers in image arrays

		/// Create the framebuffer
		VkFramebuffer framebuffer;
		VK_CHECK(vkCreateFramebuffer(this->device, &framebufferInfo, nullptr, &framebuffer));

		/// Store the framebuffer in our vector, wrapped in a VulkanFramebufferHandle for RAII
		this->swapChainFramebuffers[i] = VulkanFramebufferHandle(framebuffer, [this](VkFramebuffer fb) {
			vkDestroyFramebuffer(this->device, fb, nullptr);
		});
	}

	spdlog::info("Created {} framebuffers with color and depth attachments successfully",
		this->swapChainFramebuffers.size());
}

void FramebufferManager::recreateSwapChainFramebuffers(
	VkRenderPass renderPass,
	const std::vector<VulkanImageViewHandle>& swapChainImageViews,
	VkImageView depthImageView,
	uint32_t width,
	uint32_t height) {

	/// For framebuffer recreation, we simply delegate to createSwapChainFramebuffers
	/// This ensures the same validation and creation logic is used for both initial creation and recreation
	this->createSwapChainFramebuffers(
		renderPass,
		swapChainImageViews,
		depthImageView,
		width,
		height
	);

	spdlog::info("Recreated {} framebuffers with dimensions {}x{}",
		this->swapChainFramebuffers.size(), width, height);
}

VkFramebuffer FramebufferManager::getFramebuffer(size_t index) const {
	/// Validate the index before access to prevent out-of-bounds errors
	this->validateFramebufferIndex(index);

	/// Return the raw Vulkan handle for the requested framebuffer
	/// We don't return the RAII wrapper to maintain ownership within the manager
	return this->swapChainFramebuffers[index].get();
}

size_t FramebufferManager::getFramebufferCount() const {
	return this->swapChainFramebuffers.size();
}

bool FramebufferManager::hasFramebuffers() const {
	return !this->swapChainFramebuffers.empty();
}

void FramebufferManager::validateFramebufferIndex(size_t index) const {
	/// Check if the framebuffer vector is empty
	/// This is a common error case that deserves a specific message
	if (this->swapChainFramebuffers.empty()) {
		throw VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"No framebuffers have been created yet",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	/// Check if the index is out of bounds
	if (index >= this->swapChainFramebuffers.size()) {
		throw VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Framebuffer index " + std::to_string(index) + " out of bounds (max: " +
				std::to_string(this->swapChainFramebuffers.size() - 1) + ")",
			__FUNCTION__, __FILE__, __LINE__
		);
	}
}

} /// namespace lillugsi::vulkan