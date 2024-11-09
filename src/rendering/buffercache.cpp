#include "buffercache.h"
#include <spdlog/spdlog.h>

namespace lillugsi::rendering {

std::shared_ptr<vulkan::VertexBuffer> BufferCache::getOrCreateVertexBuffer(VkDeviceSize size) {
	/// Calculate the appropriate buffer size bucket
	VkDeviceSize bucketSize = this->calculateBufferBucket(size);

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

	/// Create buffer with device-local memory for optimal performance
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = bucketSize;  /// Use bucketed size for potential reuse
	bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkBuffer buffer;
	VK_CHECK(vkCreateBuffer(this->device, &bufferInfo, nullptr, &buffer));

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

	/// Create the vertex buffer object
	/// Note: We use bucketSize for the buffer but track the actual vertex count
	/// based on the original requested size
	auto vertexBuffer = std::make_shared<vulkan::VertexBuffer>(
		this->device,
		bufferMemory,
		std::move(bufferHandle),
		bucketSize,
		static_cast<uint32_t>(size / sizeof(Vertex)),  /// Actual vertex count from requested size
		sizeof(Vertex)
	);

	/// Store in cache and return
	this->vertexBuffers[bucketSize] = vertexBuffer;
	spdlog::info("Created new vertex buffer. Requested: {} bytes, Bucket: {} bytes",
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
	bufferInfo.size = bucketSize;  /// Use bucketed size for potential reuse
	bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkBuffer buffer;
	VK_CHECK(vkCreateBuffer(this->device, &bufferInfo, nullptr, &buffer));

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
	/// Note: We use bucketSize for the buffer but track the actual index count
	/// based on the original requested size
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

} /// namespace lillugsi::rendering