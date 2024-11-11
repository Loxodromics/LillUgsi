#pragma once

#include "mesh.h"
#include "buffercache.h"
#include "vulkan/buffer.h"
#include "vulkan/vertexbuffer.h"
#include "vulkan/indexbuffer.h"
#include <memory>
#include <vector>

namespace lillugsi::rendering {

class MeshManager {
public:
	/// Constructor taking Vulkan device references needed for buffer creation
	/// @param device The logical device for buffer operations
	/// @param physicalDevice The physical device for memory allocation
	/// @param graphicsQueue The graphics queue for transfer operations
	/// @param graphicsQueueFamilyIndex The queue family index for command pool creation
	MeshManager(VkDevice device,
		VkPhysicalDevice physicalDevice,
		VkQueue graphicsQueue,
		uint32_t graphicsQueueFamilyIndex);

	/// Destructor ensures proper cleanup of resources
	~MeshManager();

	/// Create a mesh of the specified type
	/// This method creates the mesh and its associated GPU buffers
	/// @tparam T The type of mesh to create (must derive from Mesh)
	/// @return A unique pointer to the created mesh
	template<typename T>
	std::unique_ptr<Mesh> createMesh() {
		auto mesh = std::make_unique<T>();

		/// Generate geometry and verify we have data
		mesh->generateGeometry();
		if (mesh->getVertices().empty() || mesh->getIndices().empty()) {
			throw vulkan::VulkanException(
				VK_ERROR_INITIALIZATION_FAILED,
				"Mesh generated with no geometry",
				__FUNCTION__, __FILE__, __LINE__
			);
		}

		spdlog::debug("Creating mesh with {} vertices ({} bytes) and {} indices ({} bytes)",
			mesh->getVertices().size(), mesh->getVertices().size() * sizeof(Vertex),
			mesh->getIndices().size(), mesh->getIndices().size() * sizeof(uint32_t));

		try {
			/// Create GPU buffers for the mesh
			auto vertexBuffer = this->bufferCache->getOrCreateVertexBuffer(
				mesh->getVertices().size() * sizeof(Vertex));

			auto indexBuffer = this->bufferCache->getOrCreateIndexBuffer(
				mesh->getIndices().size() * sizeof(uint32_t));

			/// Copy data to buffers using staging buffers
			this->copyToBuffer(mesh->getVertices().data(),
				mesh->getVertices().size() * sizeof(Vertex),
				vertexBuffer->get());

			this->copyToBuffer(mesh->getIndices().data(),
				mesh->getIndices().size() * sizeof(uint32_t),
				indexBuffer->get());

			/// Assign buffers to the mesh
			mesh->setBuffers(std::move(vertexBuffer), std::move(indexBuffer));

			spdlog::info("Successfully created mesh with {} vertices and {} indices",
				mesh->getVertices().size(), mesh->getIndices().size());

			return mesh;
		}
		catch (const vulkan::VulkanException& e) {
			spdlog::error("Failed to create mesh buffers: {}", e.what());
			throw;
		}
	}

	void cleanup();

private:
	/// Vulkan device references
	VkDevice device;
	VkPhysicalDevice physicalDevice;
	VkQueue graphicsQueue;

	/// Command pool for transfer operations
	VkCommandPool commandPool;

	/// Buffer cache for efficient buffer management
	std::unique_ptr<BufferCache> bufferCache;

	/// Create a command pool for transfer operations
	/// @param graphicsQueueFamilyIndex The queue family index for the command pool
	void createCommandPool(uint32_t graphicsQueueFamilyIndex);

	/// Copy data to a buffer using staging buffer
	/// @param data Pointer to the source data
	/// @param size Size of the data in bytes
	/// @param dstBuffer Destination buffer
	void copyToBuffer(const void* data, VkDeviceSize size, VkBuffer dstBuffer);

	/// Find a suitable memory type for buffer allocation
	/// This method finds a memory type that satisfies both the type requirements from Vulkan
	/// and our desired memory properties (e.g., host visible, device local)
	/// @param typeFilter Bit field of suitable memory types returned by Vulkan
	/// @param properties Required memory properties we need
	/// @return Index of a suitable memory type
	/// @throws VulkanException if no suitable memory type is found
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
};

} /// namespace lillugsi::rendering