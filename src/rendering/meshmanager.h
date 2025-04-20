#pragma once

#include "buffermanager.h"
#include "mesh.h"
#include "vulkan/buffer.h"
#include <memory>
#include <vector>

namespace lillugsi::rendering {

class MeshManager {
public:
	/// Constructor taking Vulkan device references and BufferManager
	/// @param device The logical device for buffer operations
	/// @param physicalDevice The physical device for memory allocation
	/// @param graphicsQueue The graphics queue for transfer operations
	/// @param graphicsQueueFamilyIndex The queue family index for command pool creation
	/// @param bufferManager The buffer manager to use for creating buffers
	MeshManager(VkDevice device,
	    VkPhysicalDevice physicalDevice,
	    VkQueue graphicsQueue,
	    uint32_t graphicsQueueFamilyIndex,
	    std::shared_ptr<BufferManager> bufferManager);

	/// Destructor ensures proper cleanup of resources
	~MeshManager();

	void cleanup();

	/// Create a mesh of the specified type with constructor parameters
	/// @tparam T The type of mesh to create (must derive from Mesh)
	/// @tparam Args Parameter pack for mesh constructor arguments
	/// @param args Constructor arguments forwarded to mesh creation
	/// @return A unique pointer to the created mesh
	template<typename T, typename... Args>
	[[nodiscard]] std::shared_ptr<Mesh> createMesh(Args&&... args);

	/// Update GPU buffers for a mesh
	/// @param mesh The mesh whose buffers need updating
	void updateBuffers(const std::shared_ptr<Mesh>& mesh);

	/// Update GPU buffers for a mesh if needed
	/// @param mesh The mesh whose buffers might need updating
	void updateBuffersIfNeeded(const std::shared_ptr<Mesh>& mesh);

private:
	/// Vulkan device references
	VkDevice device;
	VkPhysicalDevice physicalDevice;
	VkQueue graphicsQueue;

	/// Buffer manager for creating and updating buffers
	std::shared_ptr<BufferManager> bufferManager;
};

} /// namespace lillugsi::rendering