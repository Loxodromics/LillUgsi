/// resourcemanager.cpp

#include "resourcemanager.h"
#include <spdlog/spdlog.h>
#include <fmt/format.h>

/// Custom formatter for VkFormat to enable proper logging
template<>
struct fmt::formatter<VkFormat> : formatter<string_view> {
	template<typename FormatContext>
	auto format(VkFormat format, FormatContext& ctx) {
		string_view name = "Unknown";
		switch(format) {
			case VK_FORMAT_R8G8B8A8_SRGB: name = "VK_FORMAT_R8G8B8A8_SRGB"; break;
			case VK_FORMAT_B8G8R8A8_SRGB: name = "VK_FORMAT_B8G8R8A8_SRGB"; break;
			case VK_FORMAT_D32_SFLOAT: name = "VK_FORMAT_D32_SFLOAT"; break;
			/// Add other format cases as needed
			default: name = "Unknown Format"; break;
		}
		return formatter<string_view>::format(name, ctx);
	}
};

namespace lillugsi::vulkan {

ResourceManager::ResourceManager(VkDevice device, VkPhysicalDevice physicalDevice)
	: device(device)
	, physicalDevice(physicalDevice) {
	/// Constructor initializes the device handles
	/// We store these handles as they're necessary for creating and managing Vulkan resources
	spdlog::info("ResourceManager initialized");
}

ResourceManager::~ResourceManager() {
	/// In the destructor, we ensure all managed resources are properly cleaned up
	/// This is crucial for preventing resource leaks

	/// Clean up buffers
	for (auto& [usage, buffers] : this->bufferCache) {
		buffers.clear();  /// This will release all shared_ptrs, triggering cleanup if no other references exist
	}
	this->bufferCache.clear();

	/// Clean up images
	for (auto& [usage, images] : this->imageCache) {
		images.clear();  /// This will release all shared_ptrs, triggering cleanup if no other references exist
	}
	this->imageCache.clear();

	spdlog::info("ResourceManager cleaned up");
}

VulkanBufferHandle ResourceManager::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
	/// This public method wraps the shared_ptr logic, maintaining the existing API
	auto sharedHandle = this->createBufferShared(size, usage, properties);
	return VulkanBufferHandle(sharedHandle->get(), sharedHandle->getDeleter());
}

std::shared_ptr<VulkanBufferHandle> ResourceManager::createBufferShared(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
	/// First, check if we have a suitable buffer in the cache
	/// This can significantly improve performance by reusing existing resources
	auto& buffers = this->bufferCache[usage];
	for (auto& [cachedSize, buffer] : buffers) {
		if (cachedSize >= size) {
			/// We found a suitable buffer, return it
			spdlog::debug("Reusing cached buffer of size {} for requested size {}", cachedSize, size);
			return buffer;
		}
	}

	/// If we didn't find a suitable buffer, create a new one
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkBuffer buffer;
	VK_CHECK(vkCreateBuffer(this->device, &bufferInfo, nullptr, &buffer));

	/// Get memory requirements for the buffer
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(this->device, buffer, &memRequirements);

	/// Allocate memory for the buffer
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = this->findMemoryType(memRequirements.memoryTypeBits, properties);

	VkDeviceMemory bufferMemory;
	VK_CHECK(vkAllocateMemory(this->device, &allocInfo, nullptr, &bufferMemory));

	/// Bind the buffer to the allocated memory
	VK_CHECK(vkBindBufferMemory(this->device, buffer, bufferMemory, 0));

	/// Create a deleter function
	auto deleter = [this, bufferMemory](VkBuffer b) {
		vkFreeMemory(this->device, bufferMemory, nullptr);
		vkDestroyBuffer(this->device, b, nullptr);
	};

	/// Create a shared_ptr with the custom deleter for RAII management
	auto bufferHandle = std::make_shared<VulkanBufferHandle>(buffer, deleter);

	/// Cache the created buffer for potential future reuse
	buffers.emplace_back(size, bufferHandle);

	spdlog::info("Created new buffer. Size: {}, Usage: {}", size, usage);
	return bufferHandle;
}

VulkanImageHandle ResourceManager::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties) {
	/// This public method wraps the shared_ptr logic, maintaining the existing API
	auto sharedHandle = this->createImageShared(width, height, format, tiling, usage, properties);
	return VulkanImageHandle(sharedHandle->get(), sharedHandle->getDeleter());
}

std::shared_ptr<VulkanImageHandle> ResourceManager::createImageShared(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties) {
	/// First, check if we have a suitable image in the cache
	auto& images = this->imageCache[usage];
	for (auto& [cachedWidth, cachedHeight, cachedFormat, image] : images) {
		if (cachedWidth >= width && cachedHeight >= height && cachedFormat == format) {
			/// We found a suitable image, return it
			spdlog::debug("Reusing cached image of size {}x{} for requested size {}x{}", cachedWidth, cachedHeight, width, height);
			return image;
		}
	}

	/// If we didn't find a suitable image, create a new one
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkImage image;
	VK_CHECK(vkCreateImage(this->device, &imageInfo, nullptr, &image));

	/// Get memory requirements for the image
	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(this->device, image, &memRequirements);

	/// Allocate memory for the image
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = this->findMemoryType(memRequirements.memoryTypeBits, properties);

	VkDeviceMemory imageMemory;
	VK_CHECK(vkAllocateMemory(this->device, &allocInfo, nullptr, &imageMemory));

	/// Bind the image to the allocated memory
	VK_CHECK(vkBindImageMemory(this->device, image, imageMemory, 0));

	/// Create a deleter function
	auto deleter = [this, imageMemory](VkImage i) {
		vkFreeMemory(this->device, imageMemory, nullptr);
		vkDestroyImage(this->device, i, nullptr);
	};

	/// Create a shared_ptr with the custom deleter for RAII management
	auto imageHandle = std::make_shared<VulkanImageHandle>(image, deleter);

	/// Cache the created image for potential future reuse
	images.emplace_back(width, height, format, imageHandle);

	spdlog::info("Created new image. Size: {}x{}, Format: {}, Usage: {}", width, height, format, usage);
	return imageHandle;
}

VulkanImageViewHandle ResourceManager::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = aspectFlags;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	VkImageView imageView;
	VK_CHECK(vkCreateImageView(this->device, &viewInfo, nullptr, &imageView));

	/// Create a VulkanImageViewHandle for RAII management
	auto imageViewHandle = VulkanImageViewHandle(imageView, [this](VkImageView iv) {
		vkDestroyImageView(this->device, iv, nullptr);
	});

	spdlog::info("Created new image view. Format: {}, Aspect Flags: {}", format, aspectFlags);
	return imageViewHandle;
}

uint32_t ResourceManager::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
	/// Query the physical device for memory properties
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(this->physicalDevice, &memProperties);

	/// Iterate through the available memory types to find one that satisfies our requirements
	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	/// If we couldn't find a suitable memory type, throw an exception
	throw VulkanException(VK_ERROR_FEATURE_NOT_PRESENT, "Failed to find suitable memory type", __FUNCTION__, __FILE__, __LINE__);
}

} /// namespace lillugsi::vulkan