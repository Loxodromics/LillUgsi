#include "meshmanager.h"
#include <spdlog/spdlog.h>

namespace lillugsi::rendering {

MeshManager::MeshManager(VkDevice device,
	VkPhysicalDevice physicalDevice,
	VkQueue graphicsQueue,
	uint32_t graphicsQueueFamilyIndex)
	: device(device)
	, physicalDevice(physicalDevice)
	, graphicsQueue(graphicsQueue)
	, commandPool(VK_NULL_HANDLE)
	, bufferCache(std::make_unique<BufferCache>(device, physicalDevice)) {
	this->createCommandPool(graphicsQueueFamilyIndex);
}

MeshManager::~MeshManager() {
	this->cleanup();
}

void MeshManager::cleanup() {
	/// Clean up the buffer cache first
	if (this->bufferCache) {
		/// Log warning if we still have active buffers
		if (this->bufferCache->hasActiveBuffers()) {
			spdlog::warn("Cleaning up buffer cache with active buffers");
		}
		this->bufferCache->cleanup();
	}

	/// Destroy the command pool
	if (this->commandPool != VK_NULL_HANDLE) {
		vkDestroyCommandPool(this->device, this->commandPool, nullptr);
		this->commandPool = VK_NULL_HANDLE;
		spdlog::debug("Command pool destroyed");
	}

	spdlog::info("MeshManager cleanup completed");
}

void MeshManager::createCommandPool(uint32_t graphicsQueueFamilyIndex) {
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
	/// Use VK_COMMAND_POOL_CREATE_TRANSIENT_BIT as these command buffers
	/// will be short-lived and used only for transfer operations
	poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

	VK_CHECK(vkCreateCommandPool(this->device, &poolInfo, nullptr, &this->commandPool));
	spdlog::debug("Command pool created for transfer operations");
}

void MeshManager::copyToBuffer(const void* data, VkDeviceSize size, VkBuffer dstBuffer) {
	if (size == 0) {
		throw vulkan::VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Attempted to copy 0 bytes to buffer",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	spdlog::debug("Copying {} bytes to buffer", size);

	/// Create staging buffer
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;

	VkBufferCreateInfo stagingBufferInfo{};
	stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	stagingBufferInfo.size = size;
	stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VK_CHECK(vkCreateBuffer(this->device, &stagingBufferInfo, nullptr, &stagingBuffer));

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(this->device, stagingBuffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(
		memRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	);

	VK_CHECK(vkAllocateMemory(this->device, &allocInfo, nullptr, &stagingMemory));
	VK_CHECK(vkBindBufferMemory(this->device, stagingBuffer, stagingMemory, 0));

	/// Copy data to staging buffer
	void* mapped;
	VK_CHECK(vkMapMemory(this->device, stagingMemory, 0, size, 0, &mapped));
	memcpy(mapped, data, size);
	vkUnmapMemory(this->device, stagingMemory);

	/// Copy from staging buffer to device local buffer
	VkCommandBufferAllocateInfo cmdBufAllocInfo{};
	cmdBufAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBufAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdBufAllocInfo.commandPool = this->commandPool;
	cmdBufAllocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	VK_CHECK(vkAllocateCommandBuffers(this->device, &cmdBufAllocInfo, &commandBuffer));

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

	VkBufferCopy copyRegion{};
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, stagingBuffer, dstBuffer, 1, &copyRegion);

	VK_CHECK(vkEndCommandBuffer(commandBuffer));

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	VK_CHECK(vkQueueSubmit(this->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
	VK_CHECK(vkQueueWaitIdle(this->graphicsQueue));

	vkFreeCommandBuffers(this->device, this->commandPool, 1, &commandBuffer);

	/// Clean up staging buffer
	vkDestroyBuffer(this->device, stagingBuffer, nullptr);
	vkFreeMemory(this->device, stagingMemory, nullptr);
}

uint32_t MeshManager::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
	/// Query the physical device for available memory types
	/// This gives us information about all memory heaps (e.g., GPU memory, system memory)
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(this->physicalDevice, &memProperties);

	/// Iterate through all memory types to find one that matches our requirements
	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		/// Two conditions must be met:
		/// 1. typeFilter has a bit set for this memory type (indicates Vulkan can use it)
		/// 2. The memory type must have all the properties we need
		if ((typeFilter & (1 << i)) &&
			(memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
			}
	}

	/// If we reach here, no suitable memory type was found
	/// This is a critical error as we cannot allocate the memory we need
	throw vulkan::VulkanException(
		VK_ERROR_FEATURE_NOT_PRESENT,
		"Failed to find suitable memory type for buffer allocation",
		__FUNCTION__, __FILE__, __LINE__
	);
}

} /// namespace lillugsi::rendering