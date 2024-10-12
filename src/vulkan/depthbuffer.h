#pragma once

#include "vulkan/vulkanwrappers.h"
#include "vulkan/vulkanformatters.h"
#include "vulkan/vulkanexception.h"
#include <vulkan/vulkan.h>

namespace lillugsi::vulkan {

class DepthBuffer {
public:
	/// Constructor
	/// @param device The logical device used for creating Vulkan resources
	/// @param physicalDevice The physical device used for memory allocation
	DepthBuffer(VkDevice device, VkPhysicalDevice physicalDevice);

	/// Destructor
	~DepthBuffer();

	/// Initialize the depth buffer
	/// @param width The width of the depth buffer
	/// @param height The height of the depth buffer
	void initialize(uint32_t width, uint32_t height);

	/// Get the image view of the depth buffer
	/// @return The image view handle
	VkImageView getImageView() const { return this->imageView.get(); }

	/// Get the format of the depth buffer
	/// @return The format of the depth buffer
	VkFormat getFormat() const { return this->depthFormat; }

private:
	VkDevice device;
	VkPhysicalDevice physicalDevice;
	VkFormat depthFormat;
	vulkan::VulkanImageHandle image;
	vulkan::VulkanDeviceMemoryHandle imageMemory;
	vulkan::VulkanImageViewHandle imageView;

	void cleanup();

	/// Find a supported depth format
	/// @return A suitable depth format supported by the device
	VkFormat findSupportedFormat();

	/// Check if a format has stencil component
	/// @param format The format to check
	/// @return True if the format has a stencil component, false otherwise
	bool hasStencilComponent(VkFormat format);

	/// Find a suitable memory type for the depth buffer
	/// @param typeFilter Bit field of memory types that are suitable
	/// @param properties Required properties of the memory
	/// @return The index of a suitable memory type
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
};

} /// namespace lillugsi::vulkan