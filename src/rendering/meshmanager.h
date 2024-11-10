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
		mesh->generateGeometry();

		try {
			/// Create GPU buffers for the mesh
			auto vertexBuffer = this->bufferCache->getOrCreateVertexBuffer(
				mesh->getVertices().size() * sizeof(Vertex));

			auto indexBuffer = this->bufferCache->getOrCreateIndexBuffer(
				mesh->getIndices().size() * sizeof(uint32_t));

			/// Update buffer contents
			vertexBuffer->update(mesh->getVertices().data(),
				mesh->getVertices().size() * sizeof(Vertex));
			indexBuffer->update(mesh->getIndices().data(),
				mesh->getIndices().size() * sizeof(uint32_t));

			/// Assign buffers to the mesh
			mesh->setBuffers(std::move(vertexBuffer), std::move(indexBuffer));

			spdlog::info("Created mesh with {} vertices and {} indices",
				mesh->getVertices().size(), mesh->getIndices().size());

			return mesh;
		}
		catch (const vulkan::VulkanException& e) {
			spdlog::error("Failed to create mesh buffers: {}", e.what());
			throw;
		}
	}

	/// Clean up resources
	/// This method ensures proper cleanup of all allocated resources
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
};

} /// namespace lillugsi::rendering