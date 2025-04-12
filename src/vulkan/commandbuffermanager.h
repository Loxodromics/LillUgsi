#pragma once

#include "vulkan/vulkanwrappers.h"
#include "vulkan/vulkanexception.h"
#include <vector>
#include <unordered_map>

namespace lillugsi::vulkan {

/// CommandBufferManager centralizes the creation and management of command buffers and pools
/// This class removes command buffer management responsibilities from the Renderer, 
/// which improves separation of concerns and makes the code more maintainable.
///
/// Key benefits:
/// - Centralizes command buffer allocation logic
/// - Provides utilities for one-time command submission
/// - Tracks created command pools for proper cleanup
/// - Handles command buffer lifecycle management
class CommandBufferManager {
public:
	/// Constructor taking the logical device reference
	/// @param device The Vulkan device to use for command buffer operations
	explicit CommandBufferManager(VkDevice device);
	
	/// Destructor ensures proper cleanup of all command pools and buffers
	~CommandBufferManager();
	
	/// Initialize the command buffer manager
	/// This prepares the manager for use but doesn't create any resources yet
	/// @return True if initialization was successful, false otherwise
	[[nodiscard]] bool initialize();
	
	/// Clean up all command pools and command buffers
	/// This should be called during shutdown or when recreating the manager
	void cleanup();
	
	/// Create a command pool with the specified properties
	/// Command pools manage the memory used for command buffers
	/// @param queueFamilyIndex The queue family this pool's commands will be submitted to
	/// @param flags Optional flags for the command pool (e.g., reset capabilities)
	/// @return RAII handle to the created command pool
	[[nodiscard]] VulkanCommandPoolHandle createCommandPool(
		uint32_t queueFamilyIndex,
		VkCommandPoolCreateFlags flags = 0);
	
	/// Allocate command buffers from a command pool
	/// @param commandPool The pool to allocate buffers from
	/// @param count Number of command buffers to allocate
	/// @param level Command buffer level (primary or secondary)
	/// @return Vector of allocated command buffer handles
	[[nodiscard]] std::vector<VkCommandBuffer> allocateCommandBuffers(
		VkCommandPool commandPool,
		uint32_t count,
		VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	
	/// Begin a command buffer for one-time submission
	/// This is useful for transfer operations and other short-lived commands
	/// @param commandPool The pool to allocate the temporary command buffer from
	/// @return Command buffer ready for recording
	[[nodiscard]] VkCommandBuffer beginSingleTimeCommands(
		VkCommandPool commandPool);
	
	/// End and submit a one-time command buffer
	/// This submits the commands, waits for completion, and frees the buffer
	/// @param commandBuffer The command buffer to submit
	/// @param commandPool The pool the command buffer was allocated from
	/// @param queue The queue to submit the commands to
	void endSingleTimeCommands(
		VkCommandBuffer commandBuffer,
		VkCommandPool commandPool,
		VkQueue queue);
	
	/// Reset a command pool, allowing its command buffers to be reused
	/// This is more efficient than freeing and reallocating command buffers
	/// @param commandPool The command pool to reset
	/// @param flags Optional reset flags
	void resetCommandPool(
		VkCommandPool commandPool,
		VkCommandPoolResetFlags flags = 0);
	
	/// Free command buffers, returning their memory to the command pool
	/// @param commandPool The pool the buffers were allocated from
	/// @param commandBuffers The command buffers to free
	void freeCommandBuffers(
		VkCommandPool commandPool,
		const std::vector<VkCommandBuffer>& commandBuffers);
	
	/// Check if the manager has been initialized
	/// @return True if initialized, false otherwise
	[[nodiscard]] bool isInitialized() const { return this->initialized; }

private:
	/// The logical device used for command buffer operations
	VkDevice device;
	
	/// Initialization state flag
	bool initialized = false;
	
	/// Track created command pools for proper cleanup
	/// We use VulkanCommandPoolHandle for RAII management
	std::vector<VulkanCommandPoolHandle> commandPools;
	
	/// Map to track which command buffers were allocated from which pools
	/// This helps with proper cleanup and validation
	std::unordered_map<VkCommandPool, std::vector<VkCommandBuffer>> allocatedCommandBuffers;
};

} /// namespace lillugsi::vulkan