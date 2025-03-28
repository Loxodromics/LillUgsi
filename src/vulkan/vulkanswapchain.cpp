#include "vulkanswapchain.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace lillugsi::vulkan {
VulkanSwapchain::VulkanSwapchain()
	: swapChainImageFormat(VK_FORMAT_UNDEFINED)
	  , swapChainExtent{0, 0} {
}

void VulkanSwapchain::initialize(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, uint32_t width, uint32_t height) {
	/// Query swap chain support
	VkSurfaceCapabilitiesKHR capabilities;
	VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities));

	uint32_t formatCount;
	VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr));
	if (formatCount == 0) {
		throw VulkanException(VK_ERROR_FORMAT_NOT_SUPPORTED, "No surface formats available", __FUNCTION__, __FILE__, __LINE__);
	}
	std::vector<VkSurfaceFormatKHR> formats(formatCount);
	VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data()));

	uint32_t presentModeCount;
	VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr));
	if (presentModeCount == 0) {
		throw VulkanException(VK_ERROR_FORMAT_NOT_SUPPORTED, "No presentation modes available", __FUNCTION__, __FILE__, __LINE__);
	}
	std::vector<VkPresentModeKHR> presentModes(presentModeCount);
	VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data()));

	/// Choose swap chain settings
	VkSurfaceFormatKHR surfaceFormat = this->chooseSurfaceFormat(formats);
	VkPresentModeKHR presentMode = this->choosePresentMode(presentModes);
	VkExtent2D extent = this->chooseSwapExtent(capabilities, width, height);

	/// Determine the number of images in the swap chain
	uint32_t imageCount = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
		imageCount = capabilities.maxImageCount;
	}

	/// Create the swap chain
	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT; /// Transfer bit for screenshots
	createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.preTransform = capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	VkSwapchainKHR swapChain;
	VK_CHECK(vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain));

	this->swapChainHandle = VulkanSwapchainHandle(swapChain, [device](VkSwapchainKHR sc) { vkDestroySwapchainKHR(device, sc, nullptr); });

	/// Retrieve the swap chain images
	VK_CHECK(vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr));
	this->swapChainImages.resize(imageCount);
	VK_CHECK(vkGetSwapchainImagesKHR(device, swapChain, &imageCount, this->swapChainImages.data()));

	this->swapChainImageFormat = surfaceFormat.format;
	this->swapChainExtent = extent;

	/// Create image views
	this->createImageViews(device);

	spdlog::info("Swap chain initialized successfully");
}

VkSurfaceFormatKHR VulkanSwapchain::chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
	/// Prefer 32-bit SRGB color format
	for (const auto& availableFormat : availableFormats) {
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return availableFormat;
		}
	}

	/// If preferred format is not available, just use the first format
	return availableFormats[0];
}

VkPresentModeKHR VulkanSwapchain::choosePresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
	/// Prefer VK_PRESENT_MODE_MAILBOX_KHR for triple buffering
	for (const auto& availablePresentMode : availablePresentModes) {
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
			return availablePresentMode;
		}
	}

	/// VK_PRESENT_MODE_FIFO_KHR is always available
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanSwapchain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height) {
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return capabilities.currentExtent;
	} else {
		VkExtent2D actualExtent = {width, height};

		actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return actualExtent;
	}
}

void VulkanSwapchain::createImageViews(VkDevice device) {
	this->swapChainImageViews.resize(this->swapChainImages.size());

	for (size_t i = 0; i < this->swapChainImages.size(); i++) {
		VkImageViewCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = this->swapChainImages[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = this->swapChainImageFormat;
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		VkImageView imageView;
		VK_CHECK(vkCreateImageView(device, &createInfo, nullptr, &imageView));

		this->swapChainImageViews[i] = VulkanImageViewHandle(imageView, [device](VkImageView iv) {
			vkDestroyImageView(device, iv, nullptr);
		});
	}

	spdlog::info("Image views created successfully");
}

}
