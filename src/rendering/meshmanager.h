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

	/// Creates a mesh with pre-defined geometry data
	///
	/// Unlike procedural meshes that generate their geometry internally,
	/// imported model meshes come with complete vertex and index data.
	/// This method allows direct creation of meshes from existing geometry data
	/// without requiring a call to generateGeometry(). We use this approach
	/// for model loaders that extract geometry from files rather than
	/// generating it procedurally.
	///
	/// @tparam T The mesh class type to create
	/// @param vertices Pre-defined vertex data for the mesh
	/// @param indices Pre-defined index data for the mesh
	/// @return A shared pointer to the created mesh
	template<typename T>
	[[nodiscard]] std::shared_ptr<Mesh> createMeshWithGeometry(
		const std::vector<Vertex>& vertices,
		const std::vector<uint32_t>& indices);

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