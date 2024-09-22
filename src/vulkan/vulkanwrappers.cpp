#include "vulkanwrappers.h"
#include <stdexcept>

VkResult createVulkanInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VulkanInstanceHandle& instance) {
	VkInstance rawInstance;
	VkResult result = vkCreateInstance(pCreateInfo, pAllocator, &rawInstance);

	if (result == VK_SUCCESS) {
		instance = VulkanInstanceHandle(rawInstance, [pAllocator](VkInstance i) {
			vkDestroyInstance(i, pAllocator);
		});
	}

	return result;
}

VkResult createVulkanDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VulkanDeviceHandle& device) {
	VkDevice rawDevice;
	VkResult result = vkCreateDevice(physicalDevice, pCreateInfo, pAllocator, &rawDevice);

	if (result == VK_SUCCESS) {
		device = VulkanDeviceHandle(rawDevice, [pAllocator](VkDevice d) {
			vkDestroyDevice(d, pAllocator);
		});
	}

	return result;
}

VkResult createVulkanSwapchain(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VulkanSwapchainHandle& swapchain) {
	VkSwapchainKHR rawSwapchain;
	VkResult result = vkCreateSwapchainKHR(device, pCreateInfo, pAllocator, &rawSwapchain);

	if (result == VK_SUCCESS) {
		swapchain = VulkanSwapchainHandle(rawSwapchain, [device, pAllocator](VkSwapchainKHR s) {
			vkDestroySwapchainKHR(device, s, pAllocator);
		});
	}

	return result;
}