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

	void cleanup();

	/// Create a mesh of the specified type with constructor parameters
	/// This method creates the mesh and its associated GPU buffers
	/// The variadic template allows each mesh type to have its own parameters
	/// @tparam T The type of mesh to create (must derive from Mesh)
	/// @tparam Args Parameter pack for mesh constructor arguments
	/// @param args Constructor arguments forwarded to mesh creation
	/// @return A unique pointer to the created mesh
	template<typename T, typename... Args>
	[[nodiscard]] std::unique_ptr<Mesh> createMesh(Args&&... args);

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