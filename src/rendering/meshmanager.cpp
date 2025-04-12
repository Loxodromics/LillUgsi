#include "meshmanager.h"
#include "cubemesh.h"
#include "icospheremesh.h"

#include <spdlog/spdlog.h>

#include <utility>

namespace lillugsi::rendering {

MeshManager::MeshManager(VkDevice device,
	VkPhysicalDevice physicalDevice,
	VkQueue graphicsQueue,
	uint32_t graphicsQueueFamilyIndex,
	std::shared_ptr<vulkan::CommandBufferManager> commandBufferManager)
	: device(device)
	, physicalDevice(physicalDevice)
	, graphicsQueue(graphicsQueue)
	, commandPool(VK_NULL_HANDLE)
	, commandBufferManager(std::move(commandBufferManager))
	, bufferCache(std::make_unique<BufferCache>(device, physicalDevice)) {
	this->createCommandPool(graphicsQueueFamilyIndex);
}

MeshManager::~MeshManager() {
	this->cleanup();
}

void MeshManager::cleanup() {
	/// Clean up the buffer cache first
	if (this->bufferCache) {
		/// Log warning if we still have active buffers
		if (this->bufferCache->hasActiveBuffers()) {
			spdlog::warn("Cleaning up buffer cache with active buffers");
		}
		this->bufferCache->cleanup();
	}

	/// Destroy the command pool
	if (this->commandPool != VK_NULL_HANDLE) {
		vkDestroyCommandPool(this->device, this->commandPool, nullptr);
		this->commandPool = VK_NULL_HANDLE;
		spdlog::debug("Command pool destroyed");
	}

	spdlog::info("MeshManager cleanup completed");
}

void MeshManager::updateBuffers(const std::shared_ptr<Mesh>& mesh) {
	if (!mesh) {
		throw vulkan::VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Cannot update buffers for null mesh",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	/// Create and copy vertex buffer
	auto vertexBuffer = this->bufferCache->getOrCreateVertexBuffer(
		mesh->getVertices().size() * sizeof(Vertex));
	this->copyToBuffer(mesh->getVertices().data(),
					  mesh->getVertices().size() * sizeof(Vertex),
					  vertexBuffer->get());

	/// Create and copy index buffer
	auto indexBuffer = this->bufferCache->getOrCreateIndexBuffer(
		mesh->getIndices().size() * sizeof(uint32_t));
	this->copyToBuffer(mesh->getIndices().data(),
					  mesh->getIndices().size() * sizeof(uint32_t),
					  indexBuffer->get());

	/// Update mesh with new buffers
	mesh->setBuffers(std::move(vertexBuffer), std::move(indexBuffer));

	spdlog::debug("Updated buffers for mesh: {} vertices, {} indices",
		mesh->getVertices().size(), mesh->getIndices().size());
}

template <typename T, typename... Args>
std::shared_ptr<Mesh> MeshManager::createMesh(Args&&... args) {
	/// Create the mesh instance with provided parameters
	/// We use perfect forwarding to pass constructor arguments exactly as received
	auto mesh = std::make_unique<T>(std::forward<Args>(args)...);

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

void MeshManager::createCommandPool(uint32_t graphicsQueueFamilyIndex) {
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
	/// Use VK_COMMAND_POOL_CREATE_TRANSIENT_BIT as these command buffers
	/// will be short-lived and used only for transfer operations
	poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

	VK_CHECK(vkCreateCommandPool(this->device, &poolInfo, nullptr, &this->commandPool));
	spdlog::debug("Command pool created for transfer operations");
}

void MeshManager::copyToBuffer(const void* data, VkDeviceSize size, VkBuffer dstBuffer) {
	if (!data || size == 0) {
		throw vulkan::VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Attempted to copy 0 bytes or from null pointer to buffer",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	spdlog::debug("Copying {} bytes to buffer", size);

	/// Create staging buffer
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;

	VkBufferCreateInfo stagingBufferInfo{};
	stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	stagingBufferInfo.size = size;
	stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VK_CHECK(vkCreateBuffer(this->device, &stagingBufferInfo, nullptr, &stagingBuffer));

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(this->device, stagingBuffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(
		memRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	);

	VK_CHECK(vkAllocateMemory(this->device, &allocInfo, nullptr, &stagingMemory));
	VK_CHECK(vkBindBufferMemory(this->device, stagingBuffer, stagingMemory, 0));

	/// Copy data to staging buffer
	void* mapped;
	VK_CHECK(vkMapMemory(this->device, stagingMemory, 0, size, 0, &mapped));
	memcpy(mapped, data, size);
	vkUnmapMemory(this->device, stagingMemory);

	/// Use the command buffer manager for the transfer operation
	/// This centralizes command buffer management and ensures proper cleanup
	VkCommandBuffer commandBuffer = this->commandBufferManager->beginSingleTimeCommands(this->commandPool);

	/// Set up and record the copy command
	VkBufferCopy copyRegion{};
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, stagingBuffer, dstBuffer, 1, &copyRegion);

	/// Submit the command and wait for completion
	this->commandBufferManager->endSingleTimeCommands(
		commandBuffer,
		this->commandPool,
		this->graphicsQueue);

	/// Clean up staging resources
	vkDestroyBuffer(this->device, stagingBuffer, nullptr);
	vkFreeMemory(this->device, stagingMemory, nullptr);
}

uint32_t MeshManager::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
	/// Query the physical device for available memory types
	/// This gives us information about all memory heaps (e.g., GPU memory, system memory)
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(this->physicalDevice, &memProperties);

	/// Iterate through all memory types to find one that matches our requirements
	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		/// Two conditions must be met:
		/// 1. typeFilter has a bit set for this memory type (indicates Vulkan can use it)
		/// 2. The memory type must have all the properties we need
		if ((typeFilter & (1 << i))
			&& (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	/// If we reach here, no suitable memory type was found
	/// This is a critical error as we cannot allocate the memory we need
	throw vulkan::VulkanException(
		VK_ERROR_FEATURE_NOT_PRESENT,
		"Failed to find suitable memory type for buffer allocation",
		__FUNCTION__,
		__FILE__,
		__LINE__);
}

void MeshManager::updateBuffersIfNeeded(const std::shared_ptr<Mesh>& mesh) {
	/// Early out if no update needed
	/// This allows for efficient batching of changes
	if (!mesh || !mesh->needsBufferUpdate()) {
		return;
	}

	/// Create and copy new vertex buffer
	auto vertexBuffer = this->bufferCache->getOrCreateVertexBuffer(
		mesh->getVertices().size() * sizeof(Vertex));

	this->copyToBuffer(
		mesh->getVertices().data(),
		mesh->getVertices().size() * sizeof(Vertex),
		vertexBuffer->get());

	/// Create and copy new index buffer
	auto indexBuffer = this->bufferCache->getOrCreateIndexBuffer(
		mesh->getIndices().size() * sizeof(uint32_t));

	this->copyToBuffer(
		mesh->getIndices().data(),
		mesh->getIndices().size() * sizeof(uint32_t),
		indexBuffer->get());

	/// Update mesh with new buffers
	mesh->setBuffers(std::move(vertexBuffer), std::move(indexBuffer));

	/// Clear dirty flag now that buffers are updated
	mesh->clearBuffersDirty();

	spdlog::debug("Updated GPU buffers for mesh: {} vertices, {} indices",
		mesh->getVertices().size(), mesh->getIndices().size());
}

/// Explicit template instantiations for known mesh types
/// We need these because we're implementing the template in the cpp file
/// The double ampersands here are part of "universal references" (also called forwarding references). They allow the template to:
/// Accept both lvalues and rvalues
/// Forward them to the constructor exactly as they were passed
/// Preserve their value category(whether they can be moved fromor not)
/// In practice, you could also write it without the &&:
/// This would still work in our case because we're not doing any special move semantics with the parameters.
/// The && version is more technically correct when working with perfect forwarding, but for simple types like float
/// and int, there's no practical difference.
template std::shared_ptr<Mesh> MeshManager::createMesh<CubeMesh>();
template std::shared_ptr<Mesh> MeshManager::createMesh<IcosphereMesh, float, int>(float&&, int&&);

} /// namespace lillugsi::rendering