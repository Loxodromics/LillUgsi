#include "commandbuffermanager.h"
#include <spdlog/spdlog.h>

namespace lillugsi::vulkan {

CommandBufferManager::CommandBufferManager(VkDevice device)
	: device(device)
	, initialized(false) {
	spdlog::debug("Creating command buffer manager");
}

CommandBufferManager::~CommandBufferManager() {
	/// Ensure proper cleanup if the user forgets to call cleanup explicitly
	/// This is important because Vulkan resources must be explicitly freed
	if (this->initialized) {
		this->cleanup();
	}
}

bool CommandBufferManager::initialize() {
	/// Nothing to initialize yet, but this provides a hook for future extensions
	/// such as creating default command pools or preallocating common buffers
	this->initialized = true;
	spdlog::info("Command buffer manager initialized");
	return true;
}

void CommandBufferManager::cleanup() {
	if (!this->initialized) {
		spdlog::warn("Attempting to clean up uninitialized command buffer manager");
		return;
	}
	
	spdlog::debug("Cleaning up command buffer manager");
	
	/// Free all allocated command buffers first
	/// We need to do this before destroying the pools
	for (auto& [pool, buffers] : this->allocatedCommandBuffers) {
		if (!buffers.empty()) {
			spdlog::debug("Freeing {} command buffers from pool {}", 
				buffers.size(), (void*)pool);
			vkFreeCommandBuffers(
				this->device,
				pool,
				static_cast<uint32_t>(buffers.size()),
				buffers.data());
		}
	}
	this->allocatedCommandBuffers.clear();
	
	/// Command pools are automatically destroyed by their RAII handles
	/// We just need to clear the vector to trigger destruction
	size_t poolCount = this->commandPools.size();
	this->commandPools.clear();
	
	spdlog::info("Command buffer manager cleaned up ({} command pools)", poolCount);
	this->initialized = false;
}

VulkanCommandPoolHandle CommandBufferManager::createCommandPool(
	uint32_t queueFamilyIndex,
	VkCommandPoolCreateFlags flags) {

	if (!this->initialized) {
		throw VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Command buffer manager not initialized",
			__FUNCTION__, __FILE__, __LINE__);
	}

	/// Set up command pool creation info
	/// The queueFamilyIndex determines which queue family can use this pool's buffers
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = queueFamilyIndex;
	poolInfo.flags = flags;

	/// Create the command pool
	VkCommandPool commandPool;
	VK_CHECK(vkCreateCommandPool(this->device, &poolInfo, nullptr, &commandPool));

	/// Create RAII wrapper for the pool
	/// This ensures the pool is destroyed when no longer needed
	auto poolHandle = VulkanCommandPoolHandle(
		commandPool,
		[this](VkCommandPool pool) {
			spdlog::debug("Destroying command pool {}", (void*)pool);
			vkDestroyCommandPool(this->device, pool, nullptr);
		});

	/// Store the pool in our tracking vector
	this->commandPools.push_back(std::move(poolHandle));

	/// Initialize the tracking entry for this pool's command buffers
	this->allocatedCommandBuffers[commandPool] = {};

	spdlog::debug("Created command pool {} for queue family {}",
		(void*)commandPool, queueFamilyIndex);

	/// Create a new handle with the same raw pool and deleter
	/// We need to return a copy since we're storing one in our vector
	return VulkanCommandPoolHandle(
		commandPool,
		[this](VkCommandPool pool) {
			spdlog::debug("Destroying command pool {}", (void*)pool);
			vkDestroyCommandPool(this->device, pool, nullptr);
		});
}

std::vector<VkCommandBuffer> CommandBufferManager::allocateCommandBuffers(
	VkCommandPool commandPool,
	uint32_t count,
	VkCommandBufferLevel level) {
	
	if (!this->initialized) {
		throw VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Command buffer manager not initialized",
			__FUNCTION__, __FILE__, __LINE__);
	}
	
	/// Set up command buffer allocation info
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = commandPool;
	allocInfo.level = level;
	allocInfo.commandBufferCount = count;
	
	/// Allocate the command buffers
	std::vector<VkCommandBuffer> commandBuffers(count);
	VK_CHECK(vkAllocateCommandBuffers(
		this->device, &allocInfo, commandBuffers.data()));
	
	/// Track these command buffers for proper cleanup
	/// This helps prevent leaks if the user doesn't free them explicitly
	auto& poolBuffers = this->allocatedCommandBuffers[commandPool];
	poolBuffers.insert(
		poolBuffers.end(), 
		commandBuffers.begin(), 
		commandBuffers.end());
	
	spdlog::trace("Allocated {} command buffers from pool {}",
		count, (void*)commandPool);
	
	return commandBuffers;
}

VkCommandBuffer CommandBufferManager::beginSingleTimeCommands(
	VkCommandPool commandPool) {
	
	if (!this->initialized) {
		throw VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Command buffer manager not initialized",
			__FUNCTION__, __FILE__, __LINE__);
	}
	
	/// Allocate a single command buffer
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;
	
	VkCommandBuffer commandBuffer;
	VK_CHECK(vkAllocateCommandBuffers(this->device, &allocInfo, &commandBuffer));
	
	/// Begin the command buffer with one-time-submit flag
	/// This hints that the command buffer will be submitted once and then reset/freed
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	
	VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
	
	/// Unlike persistent command buffers, we don't track one-time buffers
	/// since they'll be freed explicitly in endSingleTimeCommands
	
	return commandBuffer;
}

void CommandBufferManager::endSingleTimeCommands(
	VkCommandBuffer commandBuffer,
	VkCommandPool commandPool,
	VkQueue queue) {
	
	if (!this->initialized) {
		throw VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Command buffer manager not initialized",
			__FUNCTION__, __FILE__, __LINE__);
	}
	
	/// End the command buffer recording
	VK_CHECK(vkEndCommandBuffer(commandBuffer));
	
	/// Set up submission info
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	
	/// Submit the commands
	/// We use a fence to ensure commands complete before continuing
	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	
	VkFence fence;
	VK_CHECK(vkCreateFence(this->device, &fenceInfo, nullptr, &fence));
	
	/// Submit and wait for completion
	VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, fence));
	VK_CHECK(vkWaitForFences(this->device, 1, &fence, VK_TRUE, UINT64_MAX));
	
	/// Clean up resources
	vkDestroyFence(this->device, fence, nullptr);
	vkFreeCommandBuffers(this->device, commandPool, 1, &commandBuffer);
}

void CommandBufferManager::resetCommandPool(
	VkCommandPool commandPool,
	VkCommandPoolResetFlags flags) {
	
	if (!this->initialized) {
		throw VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Command buffer manager not initialized",
			__FUNCTION__, __FILE__, __LINE__);
	}
	
	/// Reset the command pool
	/// This implicitly resets all command buffers allocated from it,
	/// which is more efficient than resetting each buffer individually
	VK_CHECK(vkResetCommandPool(this->device, commandPool, flags));
	
	spdlog::trace("Reset command pool {}", (void*)commandPool);
}

void CommandBufferManager::freeCommandBuffers(
	VkCommandPool commandPool,
	const std::vector<VkCommandBuffer>& commandBuffers) {
	
	if (!this->initialized) {
		throw VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Command buffer manager not initialized",
			__FUNCTION__, __FILE__, __LINE__);
	}
	
	if (commandBuffers.empty()) {
		return;
	}
	
	/// Free the command buffers
	vkFreeCommandBuffers(
		this->device,
		commandPool,
		static_cast<uint32_t>(commandBuffers.size()),
		commandBuffers.data());
	
	/// Update our tracking to remove these buffers
	auto& poolBuffers = this->allocatedCommandBuffers[commandPool];
	for (auto cmdBuffer : commandBuffers) {
		poolBuffers.erase(
			std::remove(poolBuffers.begin(), poolBuffers.end(), cmdBuffer),
			poolBuffers.end());
	}
	
	spdlog::trace("Freed {} command buffers from pool {}", 
		commandBuffers.size(), (void*)commandPool);
}

} /// namespace lillugsi::vulkan