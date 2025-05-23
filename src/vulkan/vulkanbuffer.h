#pragma once

#include "commandbuffermanager.h"
#include "vulkanexception.h"
#include "vulkanwrappers.h"
#include <vulkan/vulkan.h>

namespace lillugsi::vulkan {

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
		VkDeviceSize size,
		CommandBufferManager* cmdManager);

private:
	VkDevice device;
	VkPhysicalDevice physicalDevice;

	/// Find memory type index for the buffer
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
};

}
