#include "buffermanager.h"
#include "vulkan/vulkanexception.h"
#include <spdlog/spdlog.h>

namespace lillugsi::rendering {

BufferManager::BufferManager(
	VkDevice device,
	VkPhysicalDevice physicalDevice,
	VkQueue graphicsQueue,
	std::shared_ptr<vulkan::CommandBufferManager> commandBufferManager)
	: device(device)
	, physicalDevice(physicalDevice)
	, graphicsQueue(graphicsQueue)
	, commandBufferManager(std::move(commandBufferManager))
	, bufferCache(std::make_unique<BufferCache>(device, physicalDevice)) {
	spdlog::info("BufferManager created");
}

BufferManager::~BufferManager() {
	this->cleanup();
}

bool BufferManager::initialize(uint32_t graphicsQueueFamilyIndex) {
	if (!this->createCommandPool(graphicsQueueFamilyIndex)) {
		spdlog::error("Failed to create command pool for buffer operations");
		return false;
	}

	spdlog::info("BufferManager initialized successfully");
	return true;
}

void BufferManager::cleanup() {
	/// Clean up uniform buffers first
	this->uniformBuffers.clear();

	/// Clean up buffer cache next
	if (this->bufferCache) {
		/// Log warning if we still have active buffers
		if (this->bufferCache->hasActiveBuffers()) {
			spdlog::warn("Cleaning up buffer cache with active buffers");
		}
		this->bufferCache->cleanup();
	}

	/// Reset the command pool last
	if (this->commandPool != VK_NULL_HANDLE && this->commandBufferManager) {
		this->commandBufferManager->resetCommandPool(this->commandPool);
		spdlog::debug("Command pool reset");
		this->commandPool = VK_NULL_HANDLE;
	}

	spdlog::info("BufferManager cleanup completed");
}

std::shared_ptr<vulkan::VertexBuffer> BufferManager::createVertexBuffer(
	const std::vector<Vertex> &vertices) {
	if (vertices.empty()) {
		throw vulkan::VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Cannot create vertex buffer with empty vertex list",
			__FUNCTION__,
			__FILE__,
			__LINE__);
	}

	VkDeviceSize bufferSize = vertices.size() * sizeof(Vertex);

	/// Create a staging buffer with host-visible memory
	auto stagingBuffer = this->createStagingBuffer(bufferSize);

	/// Map and update the staging buffer
	void *mapped = stagingBuffer->map(0, bufferSize);
	memcpy(mapped, vertices.data(), bufferSize);
	stagingBuffer->unmap();

	/// Get buffer from cache or create new one (using device-local memory)
	auto vertexBuffer = this->bufferCache->getOrCreateVertexBuffer(bufferSize);

	/// Copy from staging buffer to the destination buffer
	this->copyBuffer(stagingBuffer->get(), vertexBuffer->get(), bufferSize);

	spdlog::debug("Created vertex buffer with {} vertices ({} bytes)", vertices.size(), bufferSize);

	return vertexBuffer;
}

std::shared_ptr<vulkan::IndexBuffer> BufferManager::createIndexBuffer(
	const std::vector<uint32_t> &indices) {
	if (indices.empty()) {
		throw vulkan::VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Cannot create index buffer with empty index list",
			__FUNCTION__,
			__FILE__,
			__LINE__);
	}

	VkDeviceSize bufferSize = indices.size() * sizeof(uint32_t);

	/// Create a staging buffer with host-visible memory
	auto stagingBuffer = this->createStagingBuffer(bufferSize);

	/// Map and update the staging buffer
	void *mapped = stagingBuffer->map(0, bufferSize);
	memcpy(mapped, indices.data(), bufferSize);
	stagingBuffer->unmap();

	/// Get buffer from cache or create new one (using device-local memory)
	auto indexBuffer = this->bufferCache->getOrCreateIndexBuffer(bufferSize);

	/// Copy from staging buffer to the destination buffer
	this->copyBuffer(stagingBuffer->get(), indexBuffer->get(), bufferSize);

	spdlog::debug("Created index buffer with {} indices ({} bytes)", indices.size(), bufferSize);

	return indexBuffer;
}

std::shared_ptr<vulkan::Buffer> BufferManager::createUniformBuffer(
	VkDeviceSize size,
	const void* data) {

	/// Create host-visible buffer for easy updates
	auto buffer = this->createBuffer(
		size,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	/// Initialize with data if provided
	if (data) {
		/// Use the updated updateBuffer method
		this->updateBuffer(buffer, data, size, 0);
	}

	/// Generate a unique key for tracking based on buffer handle
	std::string key = "uniform_" + std::to_string(reinterpret_cast<uint64_t>(buffer->get()));
	this->uniformBuffers[key] = buffer;

	spdlog::debug("Created uniform buffer of size {} bytes", size);

	return buffer;
}

std::shared_ptr<vulkan::Buffer> BufferManager::createStorageBuffer(
	VkDeviceSize size, const void *data) {
	/// Create host-visible buffer for easy updates
	auto buffer = this->createBuffer(
		size,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	/// Initialize with data if provided
	if (data) {
		this->updateBuffer(buffer, data, size, 0);
	}

	/// Generate a unique key for tracking based on buffer handle
	std::string key = "storage_" + std::to_string(reinterpret_cast<uint64_t>(buffer->get()));
	this->uniformBuffers[key] = buffer;

	spdlog::debug("Created storage buffer of size {} bytes", size);

	return buffer;
}

std::shared_ptr<vulkan::Buffer> BufferManager::createStagingBuffer(
	VkDeviceSize size) {

	/// Create a buffer with host-visible and host-coherent properties
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkBuffer buffer;
	VK_CHECK(vkCreateBuffer(this->device, &bufferInfo, nullptr, &buffer));

	/// Get memory requirements
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(this->device, buffer, &memRequirements);

	/// Allocate memory that is host visible
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = this->findMemoryType(
		memRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	VkDeviceMemory bufferMemory;
	VK_CHECK(vkAllocateMemory(this->device, &allocInfo, nullptr, &bufferMemory));
	VK_CHECK(vkBindBufferMemory(this->device, buffer, bufferMemory, 0));

	/// Create RAII handle
	auto bufferHandle = vulkan::VulkanBufferHandle(
		buffer,
		[this](VkBuffer b) {
			spdlog::debug("Destroying staging buffer - Handle: {}", (void*)b);
			vkDestroyBuffer(this->device, b, nullptr);
		});

	return std::make_shared<vulkan::Buffer>(
		this->device,
		bufferMemory,
		std::move(bufferHandle),
		size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
}

void BufferManager::updateBuffer(
	std::shared_ptr<vulkan::Buffer> buffer,
	const void* data,
	VkDeviceSize size,
	VkDeviceSize offset) {

	if (!buffer || !data || size == 0) {
		spdlog::warn("Attempted to update buffer with null buffer, null data, or zero size");
		return;
	}

	/// For uniform buffers that we created, we know they're host-visible
	/// We could check buffer->getUsage() & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
	/// But for now, assume if the size is small, it might be a uniform buffer we can map directly
	if (size <= 1024) { // Arbitrary threshold for uniform buffers
		try {
			void* mapped = buffer->map(offset, size);
			memcpy(mapped, data, size);
			buffer->unmap();
			spdlog::trace("Updated small buffer directly of size {} bytes at offset {}", size, offset);
			return;
		}
		catch (const vulkan::VulkanException& e) {
			spdlog::debug("Direct mapping failed, falling back to staging buffer");
			/// Fall through to staging buffer approach
		}
	}

	/// Create a staging buffer
	auto stagingBuffer = this->createStagingBuffer(size);

	/// Map and update the staging buffer
	void* mapped = stagingBuffer->map(0, size);
	memcpy(mapped, data, size);
	stagingBuffer->unmap();

	/// Copy from staging buffer to the destination buffer
	this->copyBuffer(stagingBuffer->get(), buffer->get(), size, 0, offset);

	spdlog::trace("Updated buffer via staging buffer of size {} bytes at offset {}", size, offset);
}

void BufferManager::copyBuffer(
	VkBuffer srcBuffer,
	VkBuffer dstBuffer,
	VkDeviceSize size,
	VkDeviceSize srcOffset,
	VkDeviceSize dstOffset) {

	try {
		/// Use the command buffer manager for transfer operations
		VkCommandBuffer commandBuffer = this->commandBufferManager->beginSingleTimeCommands(this->commandPool);

		/// Record copy command
		VkBufferCopy copyRegion{};
		copyRegion.srcOffset = srcOffset;
		copyRegion.dstOffset = dstOffset;
		copyRegion.size = size;
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

		/// End and submit command
		this->commandBufferManager->endSingleTimeCommands(
			commandBuffer,
			this->commandPool,
			this->graphicsQueue);

		spdlog::debug("Copied {} bytes from buffer {} to buffer {}",
			size, (void*)srcBuffer, (void*)dstBuffer);
	}
	catch (const vulkan::VulkanException& e) {
		spdlog::error("Failed to copy buffer: {}", e.what());
		throw; // Re-throw to let caller handle it
	}
}

std::shared_ptr<vulkan::Buffer> BufferManager::createBuffer(
	VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
	/// Create buffer
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkBuffer buffer;
	VK_CHECK(vkCreateBuffer(this->device, &bufferInfo, nullptr, &buffer));

	/// Get memory requirements
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(this->device, buffer, &memRequirements);

	/// Allocate memory
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = this->findMemoryType(memRequirements.memoryTypeBits, properties);

	VkDeviceMemory bufferMemory;
	VK_CHECK(vkAllocateMemory(this->device, &allocInfo, nullptr, &bufferMemory));

	/// Bind memory to buffer
	VK_CHECK(vkBindBufferMemory(this->device, buffer, bufferMemory, 0));

	/// Create RAII handle
	auto bufferHandle = vulkan::VulkanBufferHandle(buffer, [this](VkBuffer b) {
		spdlog::debug("Destroying buffer - Handle: {}", (void *) b);
		vkDestroyBuffer(this->device, b, nullptr);
	});

	/// Create and return Buffer object
	return std::make_shared<vulkan::Buffer>(
		this->device, bufferMemory, std::move(bufferHandle), size, usage);
}

bool BufferManager::createCommandPool(uint32_t graphicsQueueFamilyIndex) {
	if (!this->commandBufferManager) {
		spdlog::error("Command buffer manager is null");
		return false;
	}

	/// Create command pool through the command buffer manager
	this->commandPool
		= this->commandBufferManager
			  ->createCommandPool(graphicsQueueFamilyIndex, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);

	if (this->commandPool == VK_NULL_HANDLE) {
		spdlog::error("Failed to create command pool for buffer operations");
		return false;
	}

	spdlog::debug("Command pool created for buffer operations");
	return true;
}

uint32_t BufferManager::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
	/// Query the physical device for available memory types
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(this->physicalDevice, &memProperties);

	/// Find a memory type that satisfies our requirements
	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i))
			&& (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw vulkan::VulkanException(
		VK_ERROR_FEATURE_NOT_PRESENT,
		"Failed to find suitable memory type for buffer allocation",
		__FUNCTION__,
		__FILE__,
		__LINE__);
}

} // namespace lillugsi::rendering