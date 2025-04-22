// In meshmanager.cpp, update the implementation:

#include "meshmanager.h"
#include "cubemesh.h"
#include "icospheremesh.h"
#include <spdlog/spdlog.h>

namespace lillugsi::rendering {

MeshManager::MeshManager(
	VkDevice device,
	VkPhysicalDevice physicalDevice,
	VkQueue graphicsQueue,
	uint32_t graphicsQueueFamilyIndex,
	std::shared_ptr<BufferManager> bufferManager)
	: device(device)
	, physicalDevice(physicalDevice)
	, graphicsQueue(graphicsQueue)
	, bufferManager(std::move(bufferManager)) {
	spdlog::info("MeshManager created");
}

MeshManager::~MeshManager() {
	this->cleanup();
}

void MeshManager::cleanup() {
	/// We don't own the buffer manager, so no need to clean it up here
	spdlog::info("MeshManager cleanup completed");
}

template<typename T, typename... Args>
std::shared_ptr<Mesh> MeshManager::createMesh(Args &&...args) {
	/// Create the mesh instance with provided parameters
	auto mesh = std::make_unique<T>(std::forward<Args>(args)...);

	/// Generate geometry and verify we have data
	mesh->generateGeometry();
	if (mesh->getVertices().empty() || mesh->getIndices().empty()) {
		throw vulkan::VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Mesh generated with no geometry",
			__FUNCTION__,
			__FILE__,
			__LINE__);
	}

	spdlog::debug(
		"Creating mesh with {} vertices and {} indices",
		mesh->getVertices().size(),
		mesh->getIndices().size());

	try {
		/// Create GPU buffers using BufferManager
		auto vertexBuffer = this->bufferManager->createVertexBuffer(mesh->getVertices());
		auto indexBuffer = this->bufferManager->createIndexBuffer(mesh->getIndices());

		/// Assign buffers to the mesh
		mesh->setBuffers(std::move(vertexBuffer), std::move(indexBuffer));

		spdlog::info(
			"Successfully created mesh with {} vertices and {} indices",
			mesh->getVertices().size(),
			mesh->getIndices().size());

		return mesh;
	} catch (const vulkan::VulkanException &e) {
		spdlog::error("Failed to create mesh buffers: {}", e.what());
		throw;
	}
}

void MeshManager::updateBuffers(const std::shared_ptr<Mesh> &mesh) {
	if (!mesh) {
		throw vulkan::VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Cannot update buffers for null mesh",
			__FUNCTION__,
			__FILE__,
			__LINE__);
	}

	/// Create and update buffers using BufferManager
	auto vertexBuffer = this->bufferManager->createVertexBuffer(mesh->getVertices());
	auto indexBuffer = this->bufferManager->createIndexBuffer(mesh->getIndices());

	/// Update mesh with new buffers
	mesh->setBuffers(std::move(vertexBuffer), std::move(indexBuffer));

	spdlog::debug(
		"Updated buffers for mesh: {} vertices, {} indices",
		mesh->getVertices().size(),
		mesh->getIndices().size());
}

void MeshManager::updateBuffersIfNeeded(const std::shared_ptr<Mesh> &mesh) {
	/// Early out if no update needed
	if (!mesh || !mesh->needsBufferUpdate()) {
		return;
	}

	this->updateBuffers(mesh);

	/// Clear dirty flag now that buffers are updated
	mesh->clearBuffersDirty();
}

/// Explicit template instantiations for known mesh types
template std::shared_ptr<Mesh> MeshManager::createMesh<Mesh>();
template std::shared_ptr<Mesh> MeshManager::createMesh<CubeMesh>();
template std::shared_ptr<Mesh> MeshManager::createMesh<IcosphereMesh, float, int>(float &&, int &&);

} /// namespace lillugsi::rendering