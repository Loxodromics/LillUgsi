#pragma once

#include "vulkan/vertexbuffer.h"
#include "vulkan/indexbuffer.h"
#include <unordered_map>
#include <memory>

namespace lillugsi::rendering {

/// BufferCache manages GPU buffer lifecycle and reuse
/// This class helps prevent duplicate buffer allocations and manages buffer memory
/// It uses a bucketing system for flexible buffer reuse and ensures buffers are
/// only reused when they're not actively being used by other meshes
class BufferCache {
public:
	/// Constructor taking Vulkan device references
	/// @param device The logical device for buffer operations
	/// @param physicalDevice The physical device for memory allocation
	BufferCache(VkDevice device, VkPhysicalDevice physicalDevice)
		: device(device)
		, physicalDevice(physicalDevice)
		, minimumBufferSize(64 * 1024)  /// Start with 64KB minimum buffer size
	{
	}

	/// Get or create a vertex buffer
	/// If a suitable unused buffer exists in the cache, it will be reused
	/// @param size Required size in bytes
	/// @return Shared pointer to the vertex buffer
	std::shared_ptr<vulkan::VertexBuffer> getOrCreateVertexBuffer(VkDeviceSize size);

	/// Get or create an index buffer
	/// If a suitable unused buffer exists in the cache, it will be reused
	/// @param size Required size in bytes
	/// @return Shared pointer to the index buffer
	std::shared_ptr<vulkan::IndexBuffer> getOrCreateIndexBuffer(VkDeviceSize size);

	/// Clear all cached buffers
	/// This should be called during cleanup or when resources need to be freed
	void cleanup() {
		this->vertexBuffers.clear();
		this->indexBuffers.clear();
		spdlog::info("Buffer cache cleared");
	}

private:
	/// The logical device reference
	VkDevice device;

	/// The physical device reference
	VkPhysicalDevice physicalDevice;

	/// Minimum buffer size in bytes (64KB default)
	/// Smaller allocations will be rounded up to this size
	const VkDeviceSize minimumBufferSize;

	/// Cache of vertex buffers
	std::unordered_map<VkDeviceSize, std::shared_ptr<vulkan::VertexBuffer>> vertexBuffers;

	/// Cache of index buffers
	std::unordered_map<VkDeviceSize, std::shared_ptr<vulkan::IndexBuffer>> indexBuffers;

	/// Find a suitable memory type for buffer allocation
	/// @param typeFilter Bit field of suitable memory types
	/// @param properties Required memory properties
	/// @return Index of a suitable memory type
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

	/// Calculate the buffer size bucket for a given size
	/// This rounds up to the next power of 2, starting from minimumBufferSize
	/// @param size Requested buffer size in bytes
	/// @return Bucket size that can accommodate the requested size
	VkDeviceSize calculateBufferBucket(VkDeviceSize size) const {
		VkDeviceSize bucket = this->minimumBufferSize;
		while (bucket < size) {
			bucket *= 2;
		}
		return bucket;
	}
};

} /// namespace lillugsi::rendering