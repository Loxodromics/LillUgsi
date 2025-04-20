#pragma once

#include "buffercache.h"
#include "vulkan/buffer.h"
#include "vulkan/commandbuffermanager.h"
#include "vulkan/indexbuffer.h"
#include "vulkan/vertexbuffer.h"
#include "vulkan/vulkanwrappers.h"
#include <memory>
#include <unordered_map>

namespace lillugsi::rendering {

/*
 * BufferManager centralizes all buffer-related operations in the rendering system
 *
 * The BufferManager provides a unified interface for creating, updating, and managing
 * all GPU buffer resources. It implements different strategies for handling device-local
 * and host-visible memory to optimize performance while maintaining flexibility.
 *
 * Key Features:
 * - Centralized buffer creation and management for all buffer types
 * - Automatic handling of staging buffers for device-local memory
 * - Memory type detection and optimal allocation strategies
 * - Resource sharing through BufferCache for vertex and index buffers
 *
 * Architecture Design:
 * This class consolidates buffer management that was previously spread across multiple
 * components (Renderer, MeshManager, etc.). It uses a two-tier approach:
 *
 * 1. Direct Access: For host-visible memory (like uniform buffers), the class attempts
 *    direct mapping for efficient updates.
 * 2. Staging Buffers: For device-local memory (like vertex/index buffers), it automatically
 *    creates temporary staging buffers for data transfer.
 *
 * This design optimizes for both performance and usability:
 * - Vertex/index buffers stay in fast GPU memory for optimal rendering performance
 * - Uniform buffers use host-visible memory for frequent updates
 * - Memory transfers are handled transparently without client code changes
 * - Resource allocation follows consistent patterns for better debugging
 *
 * Implementation Notes:
 * - The class uses the CommandBufferManager for transfer operations
 * - It leverages BufferCache for efficient vertex/index buffer reuse
 * - Error handling includes fallback mechanisms when memory mapping fails
 * - Buffer destruction is managed through RAII smart pointers
 *
 * @see BufferCache, CommandBufferManager, Buffer
 * @see Vulkan Memory Model: https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/vkspec.html#memory
 */

class BufferManager {
public:
	/// Constructor taking Vulkan device references needed for buffer creation
	/// @param device The logical device for buffer operations
	/// @param physicalDevice The physical device for memory allocation
	/// @param graphicsQueue The graphics queue for transfer operations
	/// @param commandBufferManager The command buffer manager for transfer operations
	BufferManager(
		VkDevice device,
		VkPhysicalDevice physicalDevice,
		VkQueue graphicsQueue,
		std::shared_ptr<vulkan::CommandBufferManager> commandBufferManager);

	/// Destructor ensures proper cleanup of all buffer resources
	~BufferManager();

	/// Initialize the buffer manager
	/// This creates a command pool for transfer operations
	/// @param graphicsQueueFamilyIndex The queue family index for command pool creation
	/// @return True if initialization was successful
	bool initialize(uint32_t graphicsQueueFamilyIndex);

	/// Clean up all buffer resources
	/// This should be called during shutdown to ensure proper resource cleanup
	void cleanup();

	/// Create a vertex buffer with the given data
	/// @param vertices The vertex data to upload to the buffer
	/// @return A shared pointer to the created vertex buffer
	std::shared_ptr<vulkan::VertexBuffer> createVertexBuffer(const std::vector<Vertex> &vertices);

	/// Create an index buffer with the given data
	/// @param indices The index data to upload to the buffer
	/// @return A shared pointer to the created index buffer
	std::shared_ptr<vulkan::IndexBuffer> createIndexBuffer(const std::vector<uint32_t> &indices);

	/// Create a uniform buffer of the specified size
	/// @param size The size of the buffer in bytes
	/// @param data Optional pointer to initial data
	/// @return A shared pointer to the created buffer
	std::shared_ptr<vulkan::Buffer> createUniformBuffer(
		VkDeviceSize size, const void *data = nullptr);

	/// Create a storage buffer of the specified size
	/// @param size The size of the buffer in bytes
	/// @param data Optional pointer to initial data
	/// @return A shared pointer to the created buffer
	std::shared_ptr<vulkan::Buffer> createStorageBuffer(
		VkDeviceSize size, const void *data = nullptr);

	/// Create a staging buffer for temporary transfers
	/// @param size The size of the buffer in bytes
	/// @return A shared pointer to the created buffer
	std::shared_ptr<vulkan::Buffer> createStagingBuffer(VkDeviceSize size);

	/// Update a buffer with new data
	/// Uses staging buffer for device-local buffers
	/// @param buffer The buffer to update
	/// @param data Pointer to the new data
	/// @param size Size of the data in bytes
	/// @param offset Offset into the buffer in bytes
	void updateBuffer(
	    std::shared_ptr<vulkan::Buffer> buffer,
	    const void* data,
	    VkDeviceSize size,
	    VkDeviceSize offset = 0);

	/// Copy data between buffers
	/// @param srcBuffer Source buffer
	/// @param dstBuffer Destination buffer
	/// @param size Size to copy in bytes
	/// @param srcOffset Offset in source buffer
	/// @param dstOffset Offset in destination buffer
	void copyBuffer(
		VkBuffer srcBuffer,
		VkBuffer dstBuffer,
		VkDeviceSize size,
		VkDeviceSize srcOffset = 0,
		VkDeviceSize dstOffset = 0);

	/// Get the underlying buffer cache for vertex/index buffers
	/// @return Reference to the buffer cache
	BufferCache &getBufferCache() { return *this->bufferCache; }

private:
	/// Vulkan device references
	VkDevice device;
	VkPhysicalDevice physicalDevice;
	VkQueue graphicsQueue;

	/// Command pool for transfer operations
	VkCommandPool commandPool = VK_NULL_HANDLE;

	/// Command buffer manager for transfer operations
	std::shared_ptr<vulkan::CommandBufferManager> commandBufferManager;

	/// Buffer cache for vertex and index buffers
	std::unique_ptr<BufferCache> bufferCache;

	/// Map of created uniform buffers for easier management
	std::unordered_map<std::string, std::shared_ptr<vulkan::Buffer>> uniformBuffers;

	/// Create a buffer with the specified usage
	/// @param size Buffer size in bytes
	/// @param usage Buffer usage flags
	/// @param properties Memory property flags
	/// @return A shared pointer to the created buffer
	std::shared_ptr<vulkan::Buffer> createBuffer(
		VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);

	/// Create a command pool for transfer operations
	/// @param graphicsQueueFamilyIndex The queue family index for the command pool
	/// @return True if creation was successful
	bool createCommandPool(uint32_t graphicsQueueFamilyIndex);

	/// Find a suitable memory type for buffer allocation
	/// @param typeFilter Bit field of suitable memory types
	/// @param properties Required memory properties
	/// @return Index of a suitable memory type
	[[nodiscard]] uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
};

} /// namespace lillugsi::rendering