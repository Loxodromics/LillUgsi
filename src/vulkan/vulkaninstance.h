#pragma once

#include "vulkanhandle.h"
#include <vector>
#include <string>

namespace lillugsi::vulkan {
class VulkanInstance {
public:
	VulkanInstance();
	~VulkanInstance() = default;

	/// Initialize the Vulkan instance
	bool initialize(const std::vector<const char*>& requiredExtensions);

	/// Get the Vulkan instance handle
	VkInstance getInstance() const { return this->instanceWrapper.get(); }

	const std::string& getLastError() const { return this->lastError; }

private:
	/// Wrapper for the Vulkan instance
	VulkanInstanceWrapper instanceWrapper;

	/// Wrapper for the debug messenger
	VulkanHandle<VkDebugUtilsMessengerEXT, std::function<void(VkDebugUtilsMessengerEXT)>> debugMessenger;

	/// Set up debug messenger for validation layers
	void setupDebugMessenger();

	/// Check if validation layers are supported
	bool checkValidationLayerSupport();

	/// List of validation layers to enable
	std::vector<const char*> validationLayers;

	/// Flag to enable validation layers
	bool enableValidationLayers;

	std::string lastError;
};
}
