#pragma once

#include "mesh.h"
#include "buffercache.h"
#include "vulkan/buffer.h"
#include "vulkan/vertexbuffer.h"
#include "vulkan/indexbuffer.h"
#include "vulkan/vulkanhandle.h"
#include "vulkan/vulkanexception.h"
#include "vulkan/vulkanwrappers.h"
#include <glm/glm.hpp>
#include <memory>
#include <vector>

/// This class is responsible for creating and managing mesh objects
/// It provides a centralized place for mesh creation and ensures proper resource management
namespace lillugsi::rendering {

class MeshManager {
public:
	/// Constructor taking Vulkan device references needed for buffer creation
	MeshManager(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue graphicsQueue, uint32_t graphicsQueueFamilyIndex);
	~MeshManager();

	/// Create a mesh of the specified type
	template<typename T>
	std::unique_ptr<Mesh> createMesh() {
		auto mesh = std::make_unique<T>();
		mesh->generateGeometry();

		/// Create or reuse buffers for this mesh
		auto vertexBuffer = this->createVertexBufferNew(*mesh);
		auto indexBuffer = this->createIndexBufferNew(*mesh);

		/// Assign buffers to the mesh
		mesh->setBuffers(std::move(vertexBuffer), std::move(indexBuffer));

		return mesh;
	}

	/// Create a vertex buffer for a mesh (old method, marked for deprecation)
	/// @deprecated Use createVertexBufferNew instead
	[[deprecated("Use createVertexBufferNew instead")]]
	vulkan::VulkanBufferHandle createVertexBuffer(const Mesh& mesh);

	/// Create an index buffer for a mesh (old method, marked for deprecation)
	/// @deprecated Use createIndexBufferNew instead
	[[deprecated("Use createIndexBufferNew instead")]]
	vulkan::VulkanBufferHandle createIndexBuffer(const Mesh& mesh);

	/// Create a new-style vertex buffer for a mesh
	/// This method creates a strongly-typed vertex buffer with additional metadata
	/// @param mesh The mesh containing vertex data
	/// @return A shared pointer to the created vertex buffer
	std::shared_ptr<vulkan::VertexBuffer> createVertexBufferNew(const Mesh& mesh);

	/// Create a new-style index buffer for a mesh
	/// This method creates a strongly-typed index buffer with additional metadata
	/// @param mesh The mesh containing index data
	/// @return A shared pointer to the created index buffer
	std::shared_ptr<vulkan::IndexBuffer> createIndexBufferNew(const Mesh& mesh);

	void cleanup();

private:
	/// Find a suitable memory type for buffer allocation
	/// @param typeFilter Bit field of suitable memory types
	/// @param properties Required memory properties
	/// @return Index of a suitable memory type
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

	/// Create a staging buffer for data transfer
	/// @param size Size of the buffer in bytes
	/// @param data Pointer to the data to transfer
	/// @return Tuple of buffer handle and memory handle
	std::tuple<VkBuffer, VkDeviceMemory> createStagingBuffer(VkDeviceSize size, const void* data);

	/// Copy data between buffers using a command buffer
	/// @param srcBuffer Source buffer
	/// @param dstBuffer Destination buffer
	/// @param size Size of data to copy
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

	/// Create a device-local buffer
	/// @param size Buffer size in bytes
	/// @param usage Buffer usage flags
	/// @return Tuple of buffer handle and memory handle
	std::tuple<VkBuffer, VkDeviceMemory> createDeviceLocalBuffer(
		VkDeviceSize size,
		VkBufferUsageFlags usage
	);

	/// Helper function to create a buffer
	/// @param size The size of the buffer to create
	/// @param usage The intended usage of the buffer
	/// @param properties The required properties of the buffer memory
	/// @param buffer Output parameter for the created buffer
	/// @param bufferMemory Output parameter for the allocated buffer memory
	[[deprecated("Use createVertexBufferNew instead")]]
	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);

	VkDevice device;
	VkPhysicalDevice physicalDevice;
	VkQueue graphicsQueue;
	VkCommandPool commandPool;
	std::vector<vulkan::VulkanBufferHandle> createdBuffers;
	std::unique_ptr<BufferCache> bufferCache; 	/// Buffer cache for reusing GPU buffers

	/// Create a command pool for the graphics queue
	/// @param graphicsQueueFamilyIndex Index of the graphics queue family
	void createCommandPool(uint32_t graphicsQueueFamilyIndex);
};

} /// namespace lillugsi::rendering