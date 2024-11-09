#pragma once

#include "buffer.h"
#include "rendering/mesh.h"
#include <vector>

namespace lillugsi::vulkan
{
/// Specialized buffer class for index data
/// This extends the base Buffer class with index-specific functionality and metadata
/// Having a separate class prevents mixing up index and vertex buffers
class IndexBuffer : public Buffer
{
public:
	/// Create an index buffer with specified data
	/// @param device The logical device to create the buffer on
	/// @param memory The device memory allocation
	/// @param buffer The Vulkan buffer handle
	/// @param size The size of the buffer in bytes
	/// @param indexCount Number of indices in the buffer
	/// @param indexType Type of indices (usually uint16_t or uint32_t)
	IndexBuffer(VkDevice device,
	            VkDeviceMemory memory,
	            VulkanBufferHandle buffer,
	            VkDeviceSize size,
	            uint32_t indexCount,
	            VkIndexType indexType)
		: Buffer(device, memory, std::move(buffer), size,
		         VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)
		  , indexCount(indexCount)
		  , indexType(indexType)
	{
		/// Validate that the buffer size matches the index type
		VkDeviceSize expectedSize = indexCount * (indexType == VK_INDEX_TYPE_UINT16 ? 2 : 4);
		if (size < expectedSize)
		{
			throw VulkanException(
				VK_ERROR_OUT_OF_DEVICE_MEMORY,
				"Index buffer size doesn't match index type and count",
				__FUNCTION__, __FILE__, __LINE__
			);
		}
	}

	/// Get the number of indices in the buffer
	/// This is needed for draw calls
	/// @return The number of indices
	uint32_t getIndexCount() const { return this->indexCount; }

	/// Get the type of indices stored
	/// This is needed for draw calls to correctly interpret the data
	/// @return The Vulkan index type
	VkIndexType getIndexType() const { return this->indexType; }

	/// Update index data in the buffer
	/// Provides type-safe update specifically for indices
	/// @param indices Vector of index data to upload
	/// @param offset Offset into the buffer in indices (not bytes)
	/// @throws VulkanException if the update would exceed buffer bounds
	template <typename T>
	void updateIndices(const std::vector<T>& indices, uint32_t offsetIndices = 0)
	{
		/// Validate index type matches the template parameter
		if ((sizeof(T) == 2 && this->indexType != VK_INDEX_TYPE_UINT16) ||
			(sizeof(T) == 4 && this->indexType != VK_INDEX_TYPE_UINT32))
		{
			throw VulkanException(
				VK_ERROR_FORMAT_NOT_SUPPORTED,
				"Index type mismatch",
				__FUNCTION__, __FILE__, __LINE__
			);
		}

		VkDeviceSize byteOffset = offsetIndices * sizeof(T);
		VkDeviceSize updateSize = indices.size() * sizeof(T);

		if (offsetIndices + indices.size() > this->indexCount)
		{
			throw VulkanException(
				VK_ERROR_OUT_OF_DEVICE_MEMORY,
				"Index update exceeds buffer size",
				__FUNCTION__, __FILE__, __LINE__
			);
		}

		this->update(indices.data(), updateSize, byteOffset);
	}

private:
	/// Number of indices this buffer can hold
	/// Used for validation and draw calls
	uint32_t indexCount;

	/// Type of indices stored (16-bit or 32-bit)
	/// This affects memory layout and draw calls
	VkIndexType indexType;
};
} /// namespace lillugsi::vulkan
