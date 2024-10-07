#include "mesh.h"
#include "vulkan/vulkanhandle.h"
#include "vulkan/vulkanexception.h"
#include "vulkan/vulkanwrappers.h"
#include <glm/glm.hpp>
#include <vector>

/// This class is responsible for creating and managing mesh objects
/// It provides a centralized place for mesh creation and ensures proper resource management
namespace lillugsi::rendering {
class MeshManager {
public:
	/// Constructor
	/// @param device The Vulkan logical device
	/// @param physicalDevice The Vulkan physical device
	/// @param graphicsQueue The graphics queue for command submission
	/// @param graphicsQueueFamilyIndex The index of the graphics queue family
	MeshManager(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue graphicsQueue, uint32_t graphicsQueueFamilyIndex);

	/// Destructor
	~MeshManager();

	/// Create a mesh of the specified type
	/// @tparam T The type of mesh to create (must be derived from Mesh)
	/// @return A unique pointer to the created mesh
	template<typename T>
	std::unique_ptr<Mesh> createMesh() {
		return std::make_unique<T>();
	}

	/// Create vertex buffer for a mesh
	/// @param mesh The mesh for which to create the vertex buffer
	/// @return A VulkanBufferHandle containing the created vertex buffer
	vulkan::VulkanBufferHandle createVertexBuffer(const Mesh& mesh);

	/// Create index buffer for a mesh
	/// @param mesh The mesh for which to create the index buffer
	/// @return A VulkanBufferHandle containing the created index buffer
	vulkan::VulkanBufferHandle createIndexBuffer(const Mesh& mesh);

	void cleanup();

private:
	VkDevice device;                 /// The Vulkan logical device
	VkPhysicalDevice physicalDevice; /// The Vulkan physical device
	VkQueue graphicsQueue;           /// The graphics queue for command submission
	VkCommandPool commandPool;       /// Command pool for allocating command buffers
	std::vector<vulkan::VulkanBufferHandle> createdBuffers;


	/// Create a command pool for the graphics queue family
	void createCommandPool(uint32_t graphicsQueueFamilyIndex);

	/// Helper function to find a suitable memory type for a buffer
	/// @param typeFilter Bit field of memory types that are suitable
	/// @param properties Required properties of the memory
	/// @return The index of a suitable memory type
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

	/// Helper function to create a buffer
	/// @param size The size of the buffer to create
	/// @param usage The intended usage of the buffer
	/// @param properties The required properties of the buffer memory
	/// @param buffer Output parameter for the created buffer
	/// @param bufferMemory Output parameter for the allocated buffer memory
	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);

	/// Helper function to copy data between buffers
	/// @param srcBuffer The source buffer
	/// @param dstBuffer The destination buffer
	/// @param size The size of data to copy
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
};
}
