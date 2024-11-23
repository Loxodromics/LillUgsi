#include "buffercache.h"
#include <spdlog/spdlog.h>

namespace lillugsi::rendering {

std::shared_ptr<vulkan::VertexBuffer> BufferCache::getOrCreateVertexBuffer(VkDeviceSize size) {
	/// Calculate the appropriate buffer size bucket
	VkDeviceSize bucketSize = this->calculateBufferBucket(size);

	spdlog::debug("Creating vertex buffer - Requested size: {}, Bucket size: {}", size, bucketSize);

	/// Check if we have a suitable buffer in the cache
	auto it = this->vertexBuffers.find(bucketSize);
	if (it != this->vertexBuffers.end()) {
		auto& buffer = it->second;
		/// A buffer can be reused if it's only referenced by the cache
		/// (use_count == 1 means no meshes are using it)
		if (buffer.use_count() == 1) {
			spdlog::debug("Reusing vertex buffer from bucket size {} bytes", bucketSize);
			return buffer;
		}
		/// Buffer exists but is in use, create a new one in the same bucket
		spdlog::debug("Buffer of size {} exists but is in use by {} references",
			bucketSize, buffer.use_count() - 1);
	}

	/// Create staging buffer for transfer
	/// Using VK_BUFFER_USAGE_TRANSFER_SRC_BIT because this buffer will be used as the source
	/// of a transfer command
	VkBufferCreateInfo stagingBufferInfo{};
	stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	stagingBufferInfo.size = bucketSize;
	stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	spdlog::debug("Creating staging buffer - Create Info size: {}", stagingBufferInfo.size);

	VkBuffer stagingBuffer;
	VK_CHECK(vkCreateBuffer(this->device, &stagingBufferInfo, nullptr, &stagingBuffer));
	spdlog::debug("Created staging buffer - Handle: {}", (void*)stagingBuffer);

	/// Create the device-local buffer
	/// Using VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
	/// because this buffer will be the destination of a transfer and used as a vertex buffer
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = bucketSize;  /// Use bucketed size for potential reuse
	bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	spdlog::debug("Creating device buffer - Create Info size: {}", bufferInfo.size);

	VkBuffer buffer;
	VK_CHECK(vkCreateBuffer(this->device, &bufferInfo, nullptr, &buffer));
	spdlog::debug("Created device buffer - Handle: {}", (void*)buffer);

	/// Get memory requirements for staging buffer
	VkMemoryRequirements stagingMemReq{};
	vkGetBufferMemoryRequirements(this->device, stagingBuffer, &stagingMemReq);
	spdlog::debug("Staging buffer memory requirements - Size: {}", stagingMemReq.size);

	/// Get memory requirements for device buffer
	VkMemoryRequirements memRequirements{};
	vkGetBufferMemoryRequirements(this->device, buffer, &memRequirements);
	spdlog::debug("Device buffer memory requirements - Size: {}", memRequirements.size);

	/// Allocate staging memory (host visible for CPU access)
	VkMemoryAllocateInfo stagingAllocInfo{};
	stagingAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	stagingAllocInfo.allocationSize = stagingMemReq.size;
	stagingAllocInfo.memoryTypeIndex = this->findMemoryType(
		stagingMemReq.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	);

	VkDeviceMemory stagingMemory;
	VK_CHECK(vkAllocateMemory(this->device, &stagingAllocInfo, nullptr, &stagingMemory));
	VK_CHECK(vkBindBufferMemory(this->device, stagingBuffer, stagingMemory, 0));

	/// Allocate device memory (device local for best performance)
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = this->findMemoryType(
		memRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	spdlog::debug("Allocating device memory - Size: {}", allocInfo.allocationSize);

	VkDeviceMemory bufferMemory;
	VK_CHECK(vkAllocateMemory(this->device, &allocInfo, nullptr, &bufferMemory));

	/// Bind the buffer
	spdlog::debug("Binding buffer to memory");
	VK_CHECK(vkBindBufferMemory(this->device, buffer, bufferMemory, 0));

	/// Create RAII handle for the buffer
	auto bufferHandle = vulkan::VulkanBufferHandle(buffer,
		[this](VkBuffer b) {
			spdlog::debug("Destroying device buffer - Handle: {}", (void*)b);
			vkDestroyBuffer(this->device, b, nullptr);
		});

	/// Create the vertex buffer object
	/// Note: We use bucketSize for the buffer but track the actual vertex count
	/// based on the original requested size
	auto vertexBuffer = std::make_shared<vulkan::VertexBuffer>(
		this->device,
		bufferMemory,
		std::move(bufferHandle),
		bucketSize,
		static_cast<uint32_t>(size / sizeof(Vertex)),
		sizeof(Vertex)
	);

	/// Store in cache and return
	this->vertexBuffers[bucketSize] = vertexBuffer;
	spdlog::info("Successfully created vertex buffer. Requested: {} bytes, Bucket: {} bytes",
		size, bucketSize);
	return vertexBuffer;
}

std::shared_ptr<vulkan::IndexBuffer> BufferCache::getOrCreateIndexBuffer(VkDeviceSize size) {
	/// Calculate the appropriate buffer size bucket
	VkDeviceSize bucketSize = this->calculateBufferBucket(size);

	/// Check if we have a suitable buffer in the cache
	auto it = this->indexBuffers.find(bucketSize);
	if (it != this->indexBuffers.end()) {
		auto& buffer = it->second;
		/// A buffer can be reused if it's only referenced by the cache
		/// (use_count == 1 means no meshes are using it)
		if (buffer.use_count() == 1) {
			spdlog::debug("Reusing index buffer from bucket size {} bytes", bucketSize);
			return buffer;
		}
		/// Buffer exists but is in use, create a new one in the same bucket
		spdlog::debug("Buffer of size {} exists but is in use by {} references",
			bucketSize, buffer.use_count() - 1);
	}

	/// Create buffer with device-local memory for optimal performance
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkBuffer buffer;
	VK_CHECK(vkCreateBuffer(this->device, &bufferInfo, nullptr, &buffer));
	spdlog::debug("Created device index buffer - Handle: {}", (void*)buffer);

	/// Get memory requirements for this buffer
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(this->device, buffer, &memRequirements);

	/// Allocate device-local memory for optimal performance
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = this->findMemoryType(
		memRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	VkDeviceMemory bufferMemory;
	VK_CHECK(vkAllocateMemory(this->device, &allocInfo, nullptr, &bufferMemory));
	VK_CHECK(vkBindBufferMemory(this->device, buffer, bufferMemory, 0));

	/// Create RAII handle for the buffer
	auto bufferHandle = vulkan::VulkanBufferHandle(buffer,
		[this](VkBuffer b) {
			vkDestroyBuffer(this->device, b, nullptr);
		});

	/// Create the index buffer object
	auto indexBuffer = std::make_shared<vulkan::IndexBuffer>(
		this->device,
		bufferMemory,
		std::move(bufferHandle),
		bucketSize,
		static_cast<uint32_t>(size / sizeof(uint32_t)),  /// Actual index count from requested size
		VK_INDEX_TYPE_UINT32
	);

	/// Store in cache and return
	this->indexBuffers[bucketSize] = indexBuffer;
	spdlog::info("Created new index buffer. Requested: {} bytes, Bucket: {} bytes",
		size, bucketSize);
	return indexBuffer;
}

uint32_t BufferCache::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
	/// Query device for available memory types
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(this->physicalDevice, &memProperties);

	/// Find a memory type that satisfies our requirements
	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) &&
			(memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw vulkan::VulkanException(
		VK_ERROR_FEATURE_NOT_PRESENT,
		"Failed to find suitable memory type",
		__FUNCTION__, __FILE__, __LINE__
	);
}

VkDeviceSize BufferCache::calculateBufferBucket(VkDeviceSize size) const {
	spdlog::debug("Calculating buffer bucket for size: {}", size);
	VkDeviceSize bucket = this->minimumBufferSize;
	while (bucket < size) {
		bucket *= 2;
	}
	spdlog::debug("Selected bucket size: {}", bucket);
	return bucket;
}

/// Clean up all cached buffers
/// This method ensures proper cleanup of all buffer resources
/// It's crucial to call this before device destruction
void BufferCache::cleanup() {
	spdlog::debug("Starting buffer cache cleanup");
	spdlog::debug("Active vertex buffers: {}", this->vertexBuffers.size());
	spdlog::debug("Active index buffers: {}", this->indexBuffers.size());

	/// Log each buffer being cleaned up
	for (const auto& [size, buffer] : this->vertexBuffers) {
		spdlog::debug("Cleaning up vertex buffer - Size: {}, Handle: {}",
			size, (void*)buffer->get());
	}
	for (const auto& [size, buffer] : this->indexBuffers) {
		spdlog::debug("Cleaning up index buffer - Size: {}, Handle: {}",
			size, (void*)buffer->get());
	}

	/// Clear vertex buffers first
	/// The map clear will trigger buffer destruction through shared_ptr
	this->vertexBuffers.clear();
	spdlog::debug("Cleared vertex buffers");

	/// Clear index buffers
	/// The map clear will trigger buffer destruction through shared_ptr
	this->indexBuffers.clear();
	spdlog::debug("Cleared index buffers");

	spdlog::info("Buffer cache cleared");
}

/// Add method to check if we have any active buffers
/// This is useful for debugging and validation
bool BufferCache::hasActiveBuffers() const {
	return !this->vertexBuffers.empty() || !this->indexBuffers.empty();
}

} /// namespace lillugsi::rendering