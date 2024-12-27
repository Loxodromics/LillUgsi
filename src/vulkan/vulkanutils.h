#pragma once

#include "vulkanexception.h"
#include <vulkan/vulkan.h>

namespace lillugsi::vulkan::utils {

/// Find a suitable memory type that satisfies requirements and properties
/// We centralize this commonly-used function to avoid code duplication
/// and ensure consistent memory type selection across the engine
/// @param physicalDevice The physical device to query memory types from
/// @param typeFilter Bit field of suitable memory types
/// @param properties Required memory properties
/// @return Index of a suitable memory type
/// @throws VulkanException if no suitable memory type is found
[[nodiscard]] inline uint32_t findMemoryType(
	VkPhysicalDevice physicalDevice,
	uint32_t typeFilter,
	VkMemoryPropertyFlags properties) {

	/// Query device for available memory types
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

	/// Find a memory type that satisfies our requirements
	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) &&
			(memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
			}
	}

	throw VulkanException(
		VK_ERROR_FEATURE_NOT_PRESENT,
		"Failed to find suitable memory type",
		__FUNCTION__, __FILE__, __LINE__
	);
}

} /// namespace lillugsi::vulkan::utils