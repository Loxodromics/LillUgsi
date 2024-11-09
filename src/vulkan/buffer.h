#pragma once

#include "vulkanwrappers.h"
#include "vulkanexception.h"
#include <vulkan/vulkan.h>

namespace lillugsi::vulkan {

/// Base class for all GPU buffer types
/// This provides common functionality for vertex, index, uniform, and other buffers
/// We use this as a base class because all buffer types share certain properties
/// and behaviors, but have distinct purposes and additional metadata
class Buffer {
public:
	/// Constructor taking ownership of Vulkan buffer resources
	/// @param device The logical device that created the buffer
	/// @param memory The device memory allocation for this buffer
	/// @param buffer Handle to the Vulkan buffer
	/// @param size The size of the buffer in bytes
	/// @param usage The Vulkan buffer usage flags
	Buffer(VkDevice device,
		VkDeviceMemory memory,
		VulkanBufferHandle buffer,
		VkDeviceSize size,
		VkBufferUsageFlags usage)
		: device(device)
		, memory(memory)
		, buffer(std::move(buffer))
		, size(size)
		, usage(usage) {
	}

	/// Disable copying as buffers manage unique Vulkan resources
	/// This follows RAII principles and prevents accidental resource duplication
	Buffer(const Buffer&) = delete;
	Buffer& operator=(const Buffer&) = delete;

	/// Enable moving to allow buffer ownership transfer
	/// This is useful for container storage and returning buffers from functions
	Buffer(Buffer&&) noexcept = default;
	Buffer& operator=(Buffer&&) noexcept = default;

	/// Virtual destructor to ensure proper cleanup in derived classes
	virtual ~Buffer() {
		if (this->memory != VK_NULL_HANDLE) {
			vkFreeMemory(this->device, this->memory, nullptr);
		}
	}

	/// Get the raw Vulkan buffer handle
	/// This is needed for Vulkan API calls
	/// @return The underlying VkBuffer handle
	VkBuffer get() const { return this->buffer.get(); }

	/// Get the size of the buffer in bytes
	/// This is useful for validation and memory management
	/// @return The size of the buffer in bytes
	VkDeviceSize getSize() const { return this->size; }

	/// Get the buffer usage flags
	/// This allows checking how the buffer can be used
	/// @return The Vulkan buffer usage flags
	VkBufferUsageFlags getUsage() const { return this->usage; }

	/// Map the buffer memory for CPU access
	/// @param offset Offset into the buffer memory
	/// @param size Size of the region to map
	/// @return Pointer to the mapped memory
	/// @throws VulkanException if mapping fails
	void* map(VkDeviceSize offset, VkDeviceSize size) {
		void* data;
		VK_CHECK(vkMapMemory(this->device, this->memory, offset, size, 0, &data));
		return data;
	}

	/// Unmap the buffer memory
	/// This should be called after CPU access is complete
	void unmap() {
		vkUnmapMemory(this->device, this->memory);
	}

	/// Update buffer data
	/// Provides a convenient way to update buffer contents
	/// @param data Pointer to the source data
	/// @param size Size of the data in bytes
	/// @param offset Offset into the buffer
	/// @throws VulkanException if the update fails or exceeds buffer size
	void update(const void* data, VkDeviceSize size, VkDeviceSize offset = 0) {
		if (offset + size > this->size) {
			throw VulkanException(
				VK_ERROR_OUT_OF_DEVICE_MEMORY,
				"Buffer update exceeds buffer size",
				__FUNCTION__, __FILE__, __LINE__
			);
		}

		void* mapped = this->map(offset, size);
		std::memcpy(mapped, data, size);
		this->unmap();
	}

protected:
	/// The logical device that created this buffer
	/// Stored for memory management and buffer operations
	VkDevice device;

	/// The device memory allocation for this buffer
	/// We store this separately from the buffer handle for explicit memory management
	VkDeviceMemory memory;

	/// Handle to the Vulkan buffer
	/// Using our RAII wrapper for automatic cleanup
	VulkanBufferHandle buffer;

	/// Size of the buffer in bytes
	/// Stored for validation and memory management
	VkDeviceSize size;

	/// Buffer usage flags
	/// Defines how the buffer can be used in the pipeline
	VkBufferUsageFlags usage;
};

} /// namespace lillugsi::vulkan