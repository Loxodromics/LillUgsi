#include "vulkaninstance.h"
#include <spdlog/spdlog.h>

VulkanInstance::VulkanInstance() {
	/// Initialize validation layers for debug builds
	#ifdef NDEBUG
		spdlog::info("enableValidationLayers");
		this->enableValidationLayers = true;
	#else
	spdlog::info("disableValidationLayers");
		this->enableValidationLayers = false;
	#endif

	this->validationLayers = {"VK_LAYER_KHRONOS_validation"};
}

bool VulkanInstance::initialize(const std::vector<const char*>& requiredExtensions) {
	/// Check validation layer support if enabled
	if (this->enableValidationLayers && !this->checkValidationLayerSupport()) {
		spdlog::error("Validation layers requested, but not available!");
		return false;
	}

	/// Create Vulkan instance
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Vulkan Learning Renderer";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	/// Combine required extensions with additional necessary extensions
	auto extensions = requiredExtensions;

	/// Add VK_KHR_get_physical_device_properties2 extension
	/// This extension is required for VK_KHR_portability_subset, which we may need on macOS
	/// We add it here to ensure it's available if we need to use VK_KHR_portability_subset later
	extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

	/// Add debug utils extension if validation layers are enabled
	if (this->enableValidationLayers) {
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();
	createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

	/// Enable validation layers if requested
	if (this->enableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(this->validationLayers.size());
		createInfo.ppEnabledLayerNames = this->validationLayers.data();
	} else {
		createInfo.enabledLayerCount = 0;
	}

	/// Create the Vulkan instance
	VkResult result = this->instanceWrapper.create(&createInfo);
	if (result != VK_SUCCESS) {
		this->lastError = "Failed to create Vulkan instance. Error code: " + std::to_string(result);
		spdlog::error(this->lastError);
		return false;
	}

	/// Setup debug messenger if validation layers are enabled
	if (this->enableValidationLayers) {
		this->setupDebugMessenger();
	}

	return true;
}

bool VulkanInstance::checkValidationLayerSupport() {
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layerName : this->validationLayers) {
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers) {
			if (strcmp(layerName, layerProperties.layerName) == 0) {
				layerFound = true;
				break;
			}
		}

		if (!layerFound) {
			return false;
		}
	}

	return true;
}

void VulkanInstance::setupDebugMessenger() {
	/// Implementation of debug messenger setup
	/// This is a placeholder and should be implemented properly
	spdlog::info("Setting up debug messenger");
}