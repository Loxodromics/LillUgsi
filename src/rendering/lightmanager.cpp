#include "lightmanager.h"
#include <spdlog/spdlog.h>

namespace lillugsi::rendering {

uint32_t LightManager::addLight(std::shared_ptr<Light> light) {
	/// Validate input
	if (!light) {
		throw vulkan::VulkanException(
			VK_ERROR_VALIDATION_FAILED_EXT,
			"Attempted to add null light",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	/// Check light limit
	/// We enforce a maximum light count to ensure consistent performance
	/// and simplify GPU buffer management
	if (!this->canAddLight()) {
		throw vulkan::VulkanException(
			VK_ERROR_TOO_MANY_OBJECTS,
			"Maximum light count exceeded",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	/// Store the light and return its index
	this->lights.push_back(light);
	uint32_t index = static_cast<uint32_t>(this->lights.size() - 1);
	
	spdlog::debug("Added light at index {}, total lights: {}", 
		index, this->lights.size());
	
	return index;
}

void LightManager::removeLight(uint32_t index) {
	/// Validate index before removal
	/// This prevents out-of-bounds access and maintains data integrity
	if (index >= this->lights.size()) {
		throw vulkan::VulkanException(
			VK_ERROR_VALIDATION_FAILED_EXT,
			"Invalid light index",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	/// Remove the light using vector's erase method
	/// We use iterators to specify the exact element to remove
	this->lights.erase(this->lights.begin() + index);
	
	spdlog::debug("Removed light at index {}, remaining lights: {}",
		index, this->lights.size());
}

void LightManager::removeAllLights() {
	/// Clear all lights
	/// This allows for complete scene resets or cleanup
	size_t previousCount = this->lights.size();
	this->lights.clear();
	
	spdlog::debug("Removed all lights (previous count: {})", previousCount);
}

std::shared_ptr<Light> LightManager::getLight(uint32_t index) const {
	/// Validate index before access
	/// This ensures safe access to the lights vector
	if (index >= this->lights.size()) {
		throw vulkan::VulkanException(
			VK_ERROR_VALIDATION_FAILED_EXT,
			"Invalid light index",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	return this->lights[index];
}

std::vector<LightData> LightManager::getLightData() const {
	/// Create vector to hold GPU-formatted light data
	/// We prepare all light data at once for efficient GPU upload
	std::vector<LightData> lightData;
	lightData.reserve(this->lights.size());

	/// Convert each light to its GPU format
	for (const auto& light : this->lights) {
		lightData.push_back(light->getLightData());
	}

	/// Pad the buffer to MaxLights if necessary
	/// This ensures consistent buffer size for the GPU
	/// We fill unused slots with default-constructed LightData
	while (lightData.size() < MaxLights) {
		lightData.push_back(LightData{});
	}

	spdlog::trace("Prepared GPU data for {} lights (buffer size: {})",
		this->lights.size(), lightData.size());

	return lightData;
}

} /// namespace lillugsi::rendering