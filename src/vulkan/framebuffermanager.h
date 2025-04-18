#pragma once

#include "vulkanwrappers.h"
#include "vulkanexception.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace lillugsi::vulkan {

/// FramebufferManager centralizes the creation and management of Vulkan framebuffers
/// This class reduces the responsibilities of the Renderer class by encapsulating
/// all framebuffer-related operations. Using a dedicated manager improves code organization
/// and makes framebuffer lifecycle management more explicit.
class FramebufferManager {
public:
	/// Constructor taking the logical device reference
	/// @param device The Vulkan logical device used to create framebuffers
	explicit FramebufferManager(VkDevice device);

	/// Destructor ensures proper cleanup of framebuffer resources
	~FramebufferManager();

	/// Initialize the framebuffer manager
	/// This prepares the manager for use but doesn't create any framebuffers yet
	/// @return True if initialization was successful
	[[nodiscard]] bool initialize();

	/// Clean up all framebuffer resources
	/// This should be called during shutdown or when recreating all framebuffers
	void cleanup();

	/// Create framebuffers for each image in the swap chain
	/// We create one framebuffer per swap chain image, each with the same render pass
	/// but a different color attachment from the swap chain
	/// @param renderPass The render pass that these framebuffers will be compatible with
	/// @param swapChainImageViews The image views to use as color attachments
	/// @param depthImageView The image view to use as depth attachment
	/// @param width Width of the framebuffers
	/// @param height Height of the framebuffers
	void createSwapChainFramebuffers(
		VkRenderPass renderPass,
		const std::vector<VulkanImageViewHandle>& swapChainImageViews,
		VkImageView depthImageView,
		uint32_t width,
		uint32_t height);

	/// Recreate the swap chain framebuffers
	/// This is typically needed after a window resize
	/// @param renderPass The render pass that these framebuffers will be compatible with
	/// @param swapChainImageViews The new image views to use as color attachments
	/// @param depthImageView The image view to use as depth attachment
	/// @param width New width of the framebuffers
	/// @param height New height of the framebuffers
	void recreateSwapChainFramebuffers(
		VkRenderPass renderPass,
		const std::vector<VulkanImageViewHandle>& swapChainImageViews,
		VkImageView depthImageView,
		uint32_t width,
		uint32_t height);

	/// Get a framebuffer by index
	/// This provides access to framebuffers for command buffer recording
	/// @param index The index of the framebuffer to retrieve
	/// @return Handle to the requested framebuffer
	/// @throws VulkanException if the index is out of bounds
	[[nodiscard]] VkFramebuffer getFramebuffer(size_t index) const;

	/// Get the total number of framebuffers
	/// This is useful for validation and iteration
	/// @return The number of framebuffers managed by this instance
	[[nodiscard]] size_t getFramebufferCount() const;

	/// Check if any framebuffers exist
	/// This helps determine if framebuffers need to be created
	/// @return True if framebuffers exist, false otherwise
	[[nodiscard]] bool hasFramebuffers() const;

private:
	/// Logical device reference used for framebuffer operations
	VkDevice device;

	/// RAII handles for the framebuffers
	/// We use VulkanFramebufferHandle for automatic resource management
	std::vector<VulkanFramebufferHandle> swapChainFramebuffers;

	/// Track initialization state to prevent duplicate initialization
	bool initialized{false};

	/// Validate a framebuffer index before access
	/// This prevents out-of-bounds access and provides clear error messages
	/// @param index The index to validate
	/// @throws VulkanException if the index is out of bounds
	void validateFramebufferIndex(size_t index) const;
};

} /// namespace lillugsi::vulkan