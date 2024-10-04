#pragma once

#include "vulkan/vulkanwrappers.h"
#include "vulkan/vulkanexception.h"
#include <unordered_map>
#include <memory>
#include <vector>
#include <tuple>

namespace lillugsi::vulkan {

/// ResourceManager class
/// This class is responsible for creating, managing, and destroying Vulkan resources.
/// It provides a centralized point for resource management, which helps in:
/// 1. Avoiding resource leaks by ensuring proper cleanup
/// 2. Optimizing resource usage through caching and reuse
/// 3. Simplifying the main rendering code by abstracting resource management details
class ResourceManager {
public:
	/// Constructor
	/// @param device The logical Vulkan device
	/// @param physicalDevice The physical Vulkan device
	ResourceManager(VkDevice device, VkPhysicalDevice physicalDevice);

	/// Destructor
	/// Ensures all managed resources are properly cleaned up
	~ResourceManager();

	/// Create a Vulkan buffer
	/// This method creates a new buffer or returns an existing one from the cache
	/// @param size The size of the buffer in bytes
	/// @param usage The intended usage of the buffer
	/// @param properties The desired memory properties for the buffer
	/// @return A VulkanBufferHandle to the created or cached buffer
	VulkanBufferHandle createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);

	/// Create a Vulkan image
	/// This method creates a new image or returns an existing one from the cache
	/// @param width The width of the image
	/// @param height The height of the image
	/// @param format The format of the image
	/// @param tiling The tiling arrangement of the image data
	/// @param usage The intended usage of the image
	/// @param properties The desired memory properties for the image
	/// @return A VulkanImageHandle to the created or cached image
	VulkanImageHandle createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties);

	/// Create a Vulkan image view
	/// @param image The image for which to create a view
	/// @param format The format of the image
	/// @param aspectFlags The aspect(s) of the image this view will be used for
	/// @return A VulkanImageViewHandle to the created image view
	VulkanImageViewHandle createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

private:
	/// Create a shared Vulkan buffer
	/// This internal method is used by createBuffer to manage the shared_ptr logic
	/// @param size The size of the buffer in bytes
	/// @param usage The intended usage of the buffer
	/// @param properties The desired memory properties for the buffer
	/// @return A shared_ptr to the VulkanBufferHandle
	std::shared_ptr<VulkanBufferHandle> createBufferShared(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);

	/// Create a shared Vulkan image
	/// This internal method is used by createImage to manage the shared_ptr logic
	/// @param width The width of the image
	/// @param height The height of the image
	/// @param format The format of the image
	/// @param tiling The tiling arrangement of the image data
	/// @param usage The intended usage of the image
	/// @param properties The desired memory properties for the image
	/// @return A shared_ptr to the VulkanImageHandle
	std::shared_ptr<VulkanImageHandle> createImageShared(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties);

	/// Find a suitable memory type for allocation
	/// @param typeFilter Bit field of memory types that are suitable
	/// @param properties Required properties of the memory
	/// @return The index of a suitable memory type
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

	/// The logical Vulkan device
	VkDevice device;

	/// The physical Vulkan device
	VkPhysicalDevice physicalDevice;

	/// Cache for storing created buffers
	/// The key is the buffer usage flags, and the value is a vector of pairs containing
	/// the buffer size and a shared_ptr to the buffer handle
	/// This allows for efficient reuse of buffers with similar properties
	std::unordered_map<VkBufferUsageFlags, std::vector<std::pair<VkDeviceSize, std::shared_ptr<VulkanBufferHandle>>>> bufferCache;

	/// Cache for storing created images
	/// The key is the image usage flags, and the value is a vector of tuples containing
	/// the image dimensions, format, and a shared_ptr to the image handle
	/// This allows for efficient reuse of images with similar properties
	std::unordered_map<VkImageUsageFlags, std::vector<std::tuple<uint32_t, uint32_t, VkFormat, std::shared_ptr<VulkanImageHandle>>>> imageCache;
};

} /// namespace lillugsi::vulkan