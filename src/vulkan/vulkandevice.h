#pragma once

#include "vulkanwrappers.h"
#include <vector>
#include <string>

class VulkanDevice {
public:
	VulkanDevice();
	~VulkanDevice() = default;

	/// Initialize the Vulkan logical device
	bool initialize(VkPhysicalDevice physicalDevice, const std::vector<const char*>& requiredExtensions);

	/// Get the logical device handle
	VkDevice getDevice() const { return this->deviceHandle.get(); }

	/// Get the graphics queue
	VkQueue getGraphicsQueue() const { return this->graphicsQueue; }

	/// Get the present queue
	VkQueue getPresentQueue() const { return this->presentQueue; }

	/// Get the graphics queue family index
	uint32_t getGraphicsQueueFamilyIndex() const { return this->graphicsQueueFamilyIndex; }

	/// Get the last error message
	const std::string& getLastError() const { return this->lastError; }

private:
	/// Wrapper for the Vulkan logical device
	VulkanDeviceHandle deviceHandle;

	/// Graphics queue
	VkQueue graphicsQueue;

	/// Present queue
	VkQueue presentQueue;

	/// Graphics queue family index
	uint32_t graphicsQueueFamilyIndex;

	/// Last error message
	std::string lastError;

	/// Find queue families that support graphics and present operations
	bool findQueueFamilies(VkPhysicalDevice physicalDevice, uint32_t& graphicsFamily, uint32_t& presentFamily);

	/// Create logical device and retrieve queue handles
	bool createLogicalDevice(VkPhysicalDevice physicalDevice, uint32_t graphicsFamily, uint32_t presentFamily, const std::vector<const char*>& requiredExtensions);

	/// Set the last error message
	void setLastError(const std::string& error);
};