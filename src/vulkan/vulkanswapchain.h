#pragma once

#include "vulkanwrappers.h"
#include <vector>
#include <string>

class VulkanSwapchain {
public:
	VulkanSwapchain();
	~VulkanSwapchain() = default;

	/// Initialize the swap chain
	bool initialize(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, uint32_t width, uint32_t height);

	/// Get the swap chain handle
	VkSwapchainKHR getSwapChain() const { return this->swapChainHandle.get(); }

	/// Get the swap chain images
	const std::vector<VkImage>& getSwapChainImages() const { return this->swapChainImages; }

	/// Get the swap chain image views
	const std::vector<VulkanImageViewHandle>& getSwapChainImageViews() const { return this->swapChainImageViews; }

	/// Get the swap chain image format
	VkFormat getSwapChainImageFormat() const { return this->swapChainImageFormat; }

	/// Get the swap chain extent
	VkExtent2D getSwapChainExtent() const { return this->swapChainExtent; }

	/// Get the last error message
	const std::string& getLastError() const { return this->lastError; }

private:
	/// Wrapper for the Vulkan swap chain
	VulkanSwapchainHandle swapChainHandle;

	/// Swap chain images
	std::vector<VkImage> swapChainImages;

	/// Swap chain image views
	std::vector<VulkanImageViewHandle> swapChainImageViews;

	/// Swap chain image format
	VkFormat swapChainImageFormat;

	/// Swap chain extent
	VkExtent2D swapChainExtent;

	/// Last error message
	std::string lastError;

	/// Choose the surface format for the swap chain
	VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

	/// Choose the presentation mode for the swap chain
	VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

	/// Choose the swap extent (resolution) of the swap chain
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height);

	/// Create image views for the swap chain images
	bool createImageViews(VkDevice device);

	/// Set the last error message
	void setLastError(const std::string& error);
};