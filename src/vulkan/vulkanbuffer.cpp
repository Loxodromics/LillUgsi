#include "vulkanbuffer.h"
#include <spdlog/spdlog.h>

VulkanBuffer::VulkanBuffer(VkDevice device, VkPhysicalDevice physicalDevice)
	: device(device)
	, physicalDevice(physicalDevice) {
}

void VulkanBuffer::createBuffer(
	VkDeviceSize size,
	VkBufferUsageFlags usage,
	VkMemoryPropertyFlags properties,
	VulkanBufferHandle& buffer,
	VkDeviceMemory& bufferMemory
) {
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkBuffer newBuffer;
	VK_CHECK(vkCreateBuffer(this->device, &bufferInfo, nullptr, &newBuffer));

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(this->device, newBuffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = this->findMemoryType(memRequirements.memoryTypeBits, properties);

	VK_CHECK(vkAllocateMemory(this->device, &allocInfo, nullptr, &bufferMemory));
	VK_CHECK(vkBindBufferMemory(this->device, newBuffer, bufferMemory, 0));

	buffer = VulkanBufferHandle(newBuffer, [this, &bufferMemory](VkBuffer b) {
		if (b != VK_NULL_HANDLE) {
			spdlog::debug("Destroying buffer: {}", (void*)b);
			vkDestroyBuffer(this->device, b, nullptr);
		}
	});

	spdlog::info("Buffer created successfully. Size: {}, Usage: {}, Handle: {}", size, usage, (void*)newBuffer);
}

void VulkanBuffer::copyBuffer(
	VkCommandPool commandPool,
	VkQueue queue,
	VkBuffer srcBuffer,
	VkBuffer dstBuffer,
	VkDeviceSize size
) {
	/// Allocate a temporary command buffer for the transfer operation
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	VK_CHECK(vkAllocateCommandBuffers(this->device, &allocInfo, &commandBuffer));

	/// Begin command buffer recording
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

	/// Record copy command
	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0; /// Optional
	copyRegion.dstOffset = 0; /// Optional
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	/// End command buffer recording
	VK_CHECK(vkEndCommandBuffer(commandBuffer));

	/// Submit the command buffer
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
	VK_CHECK(vkQueueWaitIdle(queue));

	/// Free the temporary command buffer
	vkFreeCommandBuffers(this->device, commandPool, 1, &commandBuffer);

	spdlog::info("Buffer copy completed successfully. Size: {}", size);
}

uint32_t VulkanBuffer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(this->physicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw VulkanException(VK_ERROR_FEATURE_NOT_PRESENT, "Failed to find suitable memory type", __FUNCTION__, __FILE__, __LINE__);
}