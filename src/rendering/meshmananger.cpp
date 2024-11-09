#include "meshmananger.h"

namespace lillugsi::rendering {
MeshManager::MeshManager(VkDevice device, VkPhysicalDevice physicalDevice,
	VkQueue graphicsQueue, uint32_t graphicsQueueFamilyIndex)
	: device(device)
	, physicalDevice(physicalDevice)
	, graphicsQueue(graphicsQueue)
	, commandPool(VK_NULL_HANDLE)
	, bufferCache(std::make_unique<BufferCache>(device, physicalDevice)) {
	this->createCommandPool(graphicsQueueFamilyIndex);
}

MeshManager::~MeshManager()
{
	/// Clean up the command pool if it was created
	if (this->commandPool != VK_NULL_HANDLE)
	{
		vkDestroyCommandPool(this->device, this->commandPool, nullptr);
		this->commandPool = VK_NULL_HANDLE;
	}

	this->createdBuffers.clear(); /// This will invoke the destructors of VulkanBufferHandles

}

void MeshManager::cleanup() {

	this->bufferCache->cleanup();

	/// Destroy all created buffers
	this->createdBuffers.clear();

	/// Destroy the command pool
	if (this->commandPool != VK_NULL_HANDLE) {
		vkDestroyCommandPool(this->device, this->commandPool, nullptr);
		this->commandPool = VK_NULL_HANDLE;
		spdlog::info("MeshManager command pool destroyed");
	}

	spdlog::info("MeshManager cleanup completed");
}

void MeshManager::createCommandPool(uint32_t graphicsQueueFamilyIndex)
{
	/// Set up the command pool creation info
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
	/// VK_COMMAND_POOL_CREATE_TRANSIENT_BIT hint is used because we'll only use command buffers for short-lived operations
	poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

	/// Create the command pool
	/// We use VK_CHECK to ensure any errors are caught and handled appropriately
	VK_CHECK(vkCreateCommandPool(this->device, &poolInfo, nullptr, &this->commandPool));

	spdlog::info("Command pool created successfully for MeshManager");
}

vulkan::VulkanBufferHandle MeshManager::createVertexBuffer(const Mesh& mesh) {
	VkDeviceSize bufferSize = sizeof(Vertex) * mesh.getVertices().size();

	spdlog::info("Creating vertex buffer with size: {} bytes", bufferSize);

	if (bufferSize == 0) {
		spdlog::warn("Attempted to create a vertex buffer with size 0. Skipping buffer creation.");
		return vulkan::VulkanBufferHandle();
	}

	/// Create a staging buffer
	/// We use a staging buffer to transfer vertex data from CPU memory to GPU memory
	/// This is more efficient for the GPU to read from during rendering
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	this->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	/// Copy vertex data to the staging buffer
	void* data;
	VK_CHECK(vkMapMemory(this->device, stagingBufferMemory, 0, bufferSize, 0, &data));
	memcpy(data, mesh.getVertices().data(), (size_t) bufferSize);
	vkUnmapMemory(this->device, stagingBufferMemory);

	/// Create the vertex buffer
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;
	this->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

	/// Copy data from staging buffer to vertex buffer
	this->copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

	/// Clean up staging buffer
	vkDestroyBuffer(this->device, stagingBuffer, nullptr);
	vkFreeMemory(this->device, stagingBufferMemory, nullptr);

	/// Return the vertex buffer wrapped in a VulkanBufferHandle
	return vulkan::VulkanBufferHandle(vertexBuffer, [this, vertexBufferMemory](VkBuffer buffer) {
		vkDestroyBuffer(this->device, buffer, nullptr);
		vkFreeMemory(this->device, vertexBufferMemory, nullptr);
	});
}

vulkan::VulkanBufferHandle MeshManager::createIndexBuffer(const Mesh& mesh)
{
	VkDeviceSize bufferSize = sizeof(uint32_t) * mesh.getIndices().size();

	spdlog::info("Creating index buffer with size: {} bytes", bufferSize);

	if (bufferSize == 0) {
		spdlog::warn("Attempted to create an index buffer with size 0. Skipping buffer creation.");
		return vulkan::VulkanBufferHandle();
	}

	/// Create a staging buffer
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	this->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	/// Copy index data to the staging buffer
	void* data;
	VK_CHECK(vkMapMemory(this->device, stagingBufferMemory, 0, bufferSize, 0, &data));
	memcpy(data, mesh.getIndices().data(), (size_t) bufferSize);
	vkUnmapMemory(this->device, stagingBufferMemory);

	/// Create the index buffer
	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMemory;
	this->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

	/// Copy data from staging buffer to index buffer
	this->copyBuffer(stagingBuffer, indexBuffer, bufferSize);

	/// Clean up staging buffer
	vkDestroyBuffer(this->device, stagingBuffer, nullptr);
	vkFreeMemory(this->device, stagingBufferMemory, nullptr);

	/// Return the index buffer wrapped in a VulkanBufferHandle
	return vulkan::VulkanBufferHandle(indexBuffer, [this, indexBufferMemory](VkBuffer buffer) {
		vkDestroyBuffer(this->device, buffer, nullptr);
		vkFreeMemory(this->device, indexBufferMemory, nullptr);
	});
}

uint32_t MeshManager::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
	/// Query the physical device for memory properties
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(this->physicalDevice, &memProperties);

	/// Iterate through memory types to find one that matches our requirements
	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		/// Check if this memory type is suitable for our needs
		/// It must be in the type filter and have all the properties we require
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	/// If we couldn't find a suitable memory type, throw an exception
	throw vulkan::VulkanException(VK_ERROR_FEATURE_NOT_PRESENT, "Failed to find suitable memory type", __FUNCTION__, __FILE__, __LINE__);
}

void MeshManager::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
	/// Set up the buffer creation info
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	/// Create the buffer
	VK_CHECK(vkCreateBuffer(this->device, &bufferInfo, nullptr, &buffer));

	/// Get the memory requirements for the buffer
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(this->device, buffer, &memRequirements);

	/// Set up the memory allocation info
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = this->findMemoryType(memRequirements.memoryTypeBits, properties);

	/// Allocate memory for the buffer
	VK_CHECK(vkAllocateMemory(this->device, &allocInfo, nullptr, &bufferMemory));

	/// Bind the buffer to the allocated memory
	VK_CHECK(vkBindBufferMemory(this->device, buffer, bufferMemory, 0));
}

void MeshManager::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
	/// Set up command buffer allocation info
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = this->commandPool;
	allocInfo.commandBufferCount = 1;

	/// Allocate command buffer
	VkCommandBuffer commandBuffer;
	VK_CHECK(vkAllocateCommandBuffers(this->device, &allocInfo, &commandBuffer));

	/// Begin command buffer recording
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	/// VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT indicates that the command buffer will be rerecorded right after executing it once
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

	/// Record copy command
	VkBufferCopy copyRegion{};
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	/// End command buffer recording
	VK_CHECK(vkEndCommandBuffer(commandBuffer));

	/// Submit the command buffer
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	/// Submit to the graphics queue and wait for it to finish
	VK_CHECK(vkQueueSubmit(this->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
	VK_CHECK(vkQueueWaitIdle(this->graphicsQueue));

	/// Free the command buffer
	vkFreeCommandBuffers(this->device, this->commandPool, 1, &commandBuffer);

	spdlog::debug("Buffer copy operation completed successfully");
}

std::shared_ptr<vulkan::VertexBuffer> MeshManager::createVertexBufferNew(const Mesh& mesh) {
	const auto& vertices = mesh.getVertices();
	VkDeviceSize bufferSize = sizeof(Vertex) * vertices.size();

	if (bufferSize == 0) {
		throw vulkan::VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Attempted to create a vertex buffer with size 0",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	/// Create and populate a staging buffer
	auto [stagingBuffer, stagingMemory] = createStagingBuffer(bufferSize, vertices.data());

	/// Create a device-local buffer for the actual vertex data
	auto [vertexBuffer, vertexMemory] = createDeviceLocalBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
	);

	/// Copy data from staging buffer to vertex buffer
	copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

	/// Clean up staging buffer
	vkDestroyBuffer(this->device, stagingBuffer, nullptr);
	vkFreeMemory(this->device, stagingMemory, nullptr);

	/// Create a VulkanBufferHandle for the vertex buffer
	auto bufferHandle = vulkan::VulkanBufferHandle(vertexBuffer,
		[this](VkBuffer buffer) {
			vkDestroyBuffer(this->device, buffer, nullptr);
		}
	);

	/// Create and return the VertexBuffer
	return std::make_shared<vulkan::VertexBuffer>(
		this->device,
		vertexMemory,
		std::move(bufferHandle),
		bufferSize,
		static_cast<uint32_t>(vertices.size()),
		sizeof(Vertex)
	);
}

std::shared_ptr<vulkan::IndexBuffer> MeshManager::createIndexBufferNew(const Mesh& mesh) {
	const auto& indices = mesh.getIndices();
	VkDeviceSize bufferSize = sizeof(uint32_t) * indices.size();

	if (bufferSize == 0) {
		throw vulkan::VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Attempted to create an index buffer with size 0",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	/// Create and populate a staging buffer
	auto [stagingBuffer, stagingMemory] = createStagingBuffer(bufferSize, indices.data());

	/// Create a device-local buffer for the actual index data
	auto [indexBuffer, indexMemory] = createDeviceLocalBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
	);

	/// Copy data from staging buffer to index buffer
	copyBuffer(stagingBuffer, indexBuffer, bufferSize);

	/// Clean up staging buffer
	vkDestroyBuffer(this->device, stagingBuffer, nullptr);
	vkFreeMemory(this->device, stagingMemory, nullptr);

	/// Create a VulkanBufferHandle for the index buffer
	auto bufferHandle = vulkan::VulkanBufferHandle(indexBuffer,
		[this](VkBuffer buffer) {
			vkDestroyBuffer(this->device, buffer, nullptr);
		}
	);

	/// Create and return the IndexBuffer
	return std::make_shared<vulkan::IndexBuffer>(
		this->device,
		indexMemory,
		std::move(bufferHandle),
		bufferSize,
		static_cast<uint32_t>(indices.size()),
		VK_INDEX_TYPE_UINT32
	);
}

std::tuple<VkBuffer, VkDeviceMemory> MeshManager::createStagingBuffer(VkDeviceSize size, const void* data) {
	/// Create the staging buffer
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkBuffer stagingBuffer;
	VK_CHECK(vkCreateBuffer(this->device, &bufferInfo, nullptr, &stagingBuffer));

	/// Get memory requirements
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(this->device, stagingBuffer, &memRequirements);

	/// Allocate memory for the staging buffer
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(
		memRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	);

	VkDeviceMemory stagingMemory;
	VK_CHECK(vkAllocateMemory(this->device, &allocInfo, nullptr, &stagingMemory));

	/// Bind buffer memory
	VK_CHECK(vkBindBufferMemory(this->device, stagingBuffer, stagingMemory, 0));

	/// Copy data to staging buffer
	void* mappedMemory;
	VK_CHECK(vkMapMemory(this->device, stagingMemory, 0, size, 0, &mappedMemory));
	memcpy(mappedMemory, data, size);
	vkUnmapMemory(this->device, stagingMemory);

	return {stagingBuffer, stagingMemory};
}

std::tuple<VkBuffer, VkDeviceMemory> MeshManager::createDeviceLocalBuffer(
	VkDeviceSize size,
	VkBufferUsageFlags usage
) {
	/// Create the buffer
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;  /// Enable transfer operations
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkBuffer buffer;
	VK_CHECK(vkCreateBuffer(this->device, &bufferInfo, nullptr, &buffer));

	/// Get memory requirements
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(this->device, buffer, &memRequirements);

	/// Allocate device-local memory
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(
		memRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	VkDeviceMemory bufferMemory;
	VK_CHECK(vkAllocateMemory(this->device, &allocInfo, nullptr, &bufferMemory));

	/// Bind buffer memory
	VK_CHECK(vkBindBufferMemory(this->device, buffer, bufferMemory, 0));

	return {buffer, bufferMemory};
}

} /// namespace lillugsi::rendering
