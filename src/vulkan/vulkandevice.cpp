#include "vulkandevice.h"
#include <spdlog/spdlog.h>
#include <set>

namespace lillugsi::vulkan {
VulkanDevice::VulkanDevice()
	: graphicsQueue(VK_NULL_HANDLE)
	  , presentQueue(VK_NULL_HANDLE)
	  , graphicsQueueFamilyIndex(UINT32_MAX) {
}

void VulkanDevice::initialize(VkPhysicalDevice physicalDevice, const std::vector<const char*>& requiredExtensions) {

	/// Find queue families that support graphics and present operations
	uint32_t presentFamily;
	this->findQueueFamilies(physicalDevice, this->graphicsQueueFamilyIndex, presentFamily);

	/// Start with the required extensions
	std::vector<const char*> deviceExtensions = requiredExtensions;

	/// Check if VK_KHR_portability_subset is supported
	/// We need to do this because on some platforms (particularly macOS),
	/// this extension is necessary for Vulkan compatibility
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

	bool portabilitySubsetSupported = false;
	for (const auto& extension : availableExtensions) {
		if (strcmp(extension.extensionName, "VK_KHR_portability_subset") == 0) {
			portabilitySubsetSupported = true;
			break;
		}
	}

	/// If VK_KHR_portability_subset is supported, add it to our list of extensions
	/// This ensures compatibility on platforms that require it (like macOS)
	if (portabilitySubsetSupported) {
		deviceExtensions.push_back("VK_KHR_portability_subset");
		spdlog::info("VK_KHR_portability_subset extension enabled");
	}

	/// Log the extensions we're going to enable
	spdlog::info("Enabling the following device extensions:");
	for (const auto& extension : deviceExtensions) {
		spdlog::info("  {}", extension);
	}

	/// Create the logical device
	this->createLogicalDevice(physicalDevice, this->graphicsQueueFamilyIndex, presentFamily, deviceExtensions);
}

void VulkanDevice::findQueueFamilies(VkPhysicalDevice physicalDevice, uint32_t& graphicsFamily, uint32_t& presentFamily) {
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

	/// Find a queue family that supports graphics operations
	graphicsFamily = UINT32_MAX;
	presentFamily = UINT32_MAX;
	for (uint32_t i = 0; i < queueFamilyCount; i++) {
		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			graphicsFamily = i;
			/// For simplicity, we'll use the same queue for present operations
			presentFamily = i;
			break;
		}
	}

	if (graphicsFamily == UINT32_MAX || presentFamily == UINT32_MAX) {
		throw VulkanException(VK_ERROR_FEATURE_NOT_PRESENT, "Failed to find suitable queue families", __FUNCTION__, __FILE__, __LINE__);
	}
}

void VulkanDevice::createLogicalDevice(VkPhysicalDevice physicalDevice, uint32_t graphicsFamily, uint32_t presentFamily, const std::vector<const char*>& requiredExtensions) {
	/// First, query device features
	VkPhysicalDeviceFeatures deviceFeatures{};
	vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

	/// Check if non-solid fill modes are supported
	if (!deviceFeatures.fillModeNonSolid) {
		spdlog::warn("Device does not support non-solid fill modes (wireframe rendering may not be available)");
	}

	/// Specify the queues to be created
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = {graphicsFamily, presentFamily};

	float queuePriority = 1.0f;
	for (uint32_t queueFamily : uniqueQueueFamilies) {
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	/// Specify device features we want to use
	VkPhysicalDeviceFeatures enabledFeatures{};
	enabledFeatures.fillModeNonSolid = VK_TRUE;  /// Enable non-solid fill modes for wireframe rendering

	/// Create the logical device
	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.pEnabledFeatures = &deviceFeatures;
	createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
	createInfo.ppEnabledExtensionNames = requiredExtensions.data();

	VkDevice device;
	VK_CHECK(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device));

	/// Wrap the device in our RAII wrapper
	this->deviceHandle = VulkanDeviceHandle(device, [](VkDevice d) { vkDestroyDevice(d, nullptr); });

	/// Get queue handles
	vkGetDeviceQueue(device, graphicsFamily, 0, &this->graphicsQueue);
	vkGetDeviceQueue(device, presentFamily, 0, &this->presentQueue);

	spdlog::info("Logical device created successfully");
}

}
