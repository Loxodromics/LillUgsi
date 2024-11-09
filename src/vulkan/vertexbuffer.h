#pragma once

#include "buffer.h"
#include "mesh.h"
#include <vector>

namespace lillugsi::vulkan
{
/// Specialized buffer class for vertex data
/// This extends the base Buffer class with vertex-specific functionality and metadata
/// The specialized class ensures type safety and keeps vertex-related data together
class VertexBuffer : public Buffer
{
public:
	/// Create a vertex buffer with specified data
	/// @param device The logical device to create the buffer on
	/// @param memory The device memory allocation
	/// @param buffer The Vulkan buffer handle
	/// @param size The size of the buffer in bytes
	/// @param vertexCount Number of vertices in the buffer
	/// @param stride Size of each vertex in bytes
	VertexBuffer(VkDevice device,
	             VkDeviceMemory memory,
	             VulkanBufferHandle buffer,
	             VkDeviceSize size,
	             uint32_t vertexCount,
	             uint32_t stride)
		: Buffer(device, memory, std::move(buffer), size,
		         VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)
		  , vertexCount(vertexCount)
		  , stride(stride)
	{
	}

	/// Get the number of vertices in the buffer
	/// This is useful for draw calls and validation
	/// @return The number of vertices
	uint32_t getVertexCount() const { return this->vertexCount; }

	/// Get the stride (size of each vertex)
	/// This is needed for vertex binding descriptors
	/// @return The stride in bytes
	uint32_t getStride() const { return this->stride; }

	/// Update vertex data in the buffer
	/// Provides type-safe update specifically for vertices
	/// @param vertices Vector of vertex data to upload
	/// @param offset Offset into the buffer in vertices (not bytes)
	/// @throws VulkanException if the update would exceed buffer bounds
	void updateVertices(const std::vector<rendering::Vertex>& vertices, uint32_t offsetVertices = 0)
	{
		VkDeviceSize byteOffset = offsetVertices * this->stride;
		VkDeviceSize updateSize = vertices.size() * this->stride;

		if (offsetVertices + vertices.size() > this->vertexCount)
		{
			throw VulkanException(
				VK_ERROR_OUT_OF_DEVICE_MEMORY,
				"Vertex update exceeds buffer size",
				__FUNCTION__, __FILE__, __LINE__
			);
		}

		this->update(vertices.data(), updateSize, byteOffset);
	}

private:
	/// Number of vertices this buffer can hold
	/// Used for validation and draw calls
	uint32_t vertexCount;

	/// Size of each vertex in bytes
	/// Used for offset calculations and memory layout
	uint32_t stride;
};
} /// namespace lillugsi::vulkan
