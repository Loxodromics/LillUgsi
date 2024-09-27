#pragma once

#include "vulkanwrappers.h"
#include "vulkanexception.h"
#include <vulkan/vulkan.h>

class VulkanBuffer {
public:
	VulkanBuffer(VkDevice device, VkPhysicalDevice physicalDevice);
	~VulkanBuffer() = default;

	/// Create a Vulkan buffer
	void createBuffer(
		VkDeviceSize size,
		VkBufferUsageFlags usage,
		VkMemoryPropertyFlags properties,
		VulkanBufferHandle& buffer,
		VkDeviceMemory& bufferMemory
	);

	/// Copy data between buffers
	void copyBuffer(
		VkCommandPool commandPool,
		VkQueue queue,
		VkBuffer srcBuffer,
		VkBuffer dstBuffer,
		VkDeviceSize size
	);

private:
	VkDevice device;
	VkPhysicalDevice physicalDevice;

	/// Find memory type index for the buffer
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
};