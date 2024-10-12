#include "depthbuffer.h"
#include <array>
#include <spdlog/spdlog.h>

namespace lillugsi::vulkan {

DepthBuffer::DepthBuffer(VkDevice device, VkPhysicalDevice physicalDevice)
	: device(device)
	, physicalDevice(physicalDevice)
	, depthFormat(VK_FORMAT_UNDEFINED) {
}

DepthBuffer::~DepthBuffer() {
	this->cleanup();
}

void DepthBuffer::cleanup() {
	/// The image, image view, and memory will be automatically cleaned up
	/// by their respective RAII wrappers when they go out of scope
	this->imageView.reset();
	this->image.reset();
	this->imageMemory.reset();

	spdlog::info("Depth buffer resources cleaned up");
}

void DepthBuffer::initialize(uint32_t width, uint32_t height) {
	/// Find a suitable depth format
	/// We do this first because the format affects how we create the image
	this->depthFormat = this->findSupportedFormat();
	spdlog::info("Selected depth format: {}", this->depthFormat);

	/// Create the depth image
	/// We use VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT to indicate that this image
	/// will be used as a depth (and possibly stencil) attachment in a framebuffer
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = this->depthFormat;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;  /// Use optimal tiling for best performance on GPU
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;  /// We don't care about initial layout, will transition later
	imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;  /// This image will be used as a depth/stencil attachment
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;  /// No multisampling for depth buffer
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;  /// Image will be used by one queue family

	VkImage depthImage;
	VK_CHECK(vkCreateImage(this->device, &imageInfo, nullptr, &depthImage));

	/// Wrap the depth image in our RAII wrapper
	this->image = vulkan::VulkanImageHandle(depthImage, [this](VkImage image) {
		vkDestroyImage(this->device, image, nullptr);
	});

	/// Get memory requirements for the depth image
	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(this->device, this->image.get(), &memRequirements);

	/// Allocate memory for the depth image
	/// We use VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT for optimal performance,
	/// as the depth buffer will only be accessed by the GPU
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = this->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VkDeviceMemory tempImageMemory;
	VK_CHECK(vkAllocateMemory(this->device, &allocInfo, nullptr, &tempImageMemory));

	/// Wrap the allocated memory in our RAII wrapper
	this->imageMemory = vulkan::VulkanDeviceMemoryHandle(tempImageMemory, [this](VkDeviceMemory memory)
	{
		vkFreeMemory(this->device, memory, nullptr);
	});

	/// Bind the allocated memory to the depth image
	VK_CHECK(vkBindImageMemory(this->device, this->image.get(), this->imageMemory.get(), 0));

	/// Bind the allocated memory to the depth image
	VK_CHECK(vkBindImageMemory(this->device, this->image.get(), this->imageMemory, 0));

	/// Create the image view for the depth image
	/// The image view is necessary to use the image as a framebuffer attachment
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = this->image.get();
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = this->depthFormat;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	/// If the depth format contains a stencil component, we need to include it in the view
	if (this->hasStencilComponent(this->depthFormat)) {
		viewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	VkImageView depthImageView;
	VK_CHECK(vkCreateImageView(this->device, &viewInfo, nullptr, &depthImageView));

	/// Wrap the depth image view in our RAII wrapper
	this->imageView = vulkan::VulkanImageViewHandle(depthImageView, [this](VkImageView view) {
		vkDestroyImageView(this->device, view, nullptr);
	});

	spdlog::info("Depth buffer initialized successfully");
}

VkFormat DepthBuffer::findSupportedFormat() {
	/// We prefer these formats in order:
	/// 1. 32-bit float for higher precision
	/// 2. 24-bit with 8-bit stencil for compatibility
	/// 3. 16-bit float for lower memory usage
	std::array<VkFormat, 3> candidates = {
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D16_UNORM
	};

	/// We need to check for both the format support and the linear tiling feature
	/// VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT is required for depth attachment usage
	for (VkFormat format : candidates) {
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(this->physicalDevice, format, &props);

		if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
			return format;
		}
	}

	/// If we reach here, no suitable format was found
	throw vulkan::VulkanException(VK_ERROR_FORMAT_NOT_SUPPORTED, "Failed to find supported depth format", __FUNCTION__, __FILE__, __LINE__);
}

bool DepthBuffer::hasStencilComponent(VkFormat format) {
	/// Check if the format includes a stencil component
	/// This is important for correctly setting up image views and render passes
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

uint32_t DepthBuffer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
	/// Query the physical device for memory properties
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(this->physicalDevice, &memProperties);

	/// Iterate through memory types to find one that matches our requirements
	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	/// If we reach here, no suitable memory type was found
	throw vulkan::VulkanException(VK_ERROR_FEATURE_NOT_PRESENT, "Failed to find suitable memory type", __FUNCTION__, __FILE__, __LINE__);
}

} /// namespace lillugsi::vulkan