#include "meshmanager.h"
#include <spdlog/spdlog.h>

namespace lillugsi::rendering {

MeshManager::MeshManager(VkDevice device,
	VkPhysicalDevice physicalDevice,
	VkQueue graphicsQueue,
	uint32_t graphicsQueueFamilyIndex)
	: device(device)
	, physicalDevice(physicalDevice)
	, graphicsQueue(graphicsQueue)
	, commandPool(VK_NULL_HANDLE)
	, bufferCache(std::make_unique<BufferCache>(device, physicalDevice)) {
	this->createCommandPool(graphicsQueueFamilyIndex);
}

MeshManager::~MeshManager() {
	this->cleanup();
}

void MeshManager::cleanup() {
	/// Clean up the buffer cache first
	if (this->bufferCache) {
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

} /// namespace lillugsi::rendering