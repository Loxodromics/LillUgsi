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

LightBufferUBO LightManager::getLightBufferUBO() const {
	/// Create the complete light buffer structure
	/// This includes both the light array and the active light count
	LightBufferUBO ubo{};

	/// Convert each active light to its GPU format
	/// We populate the array up to the number of active lights
	for (size_t i = 0; i < this->lights.size(); ++i) {
		ubo.lights[i] = this->lights[i]->getLightData();
	}

	/// Set the active light count
	/// Shaders will use this to avoid iterating over inactive lights
	ubo.lightCount = static_cast<uint32_t>(this->lights.size());

	/// Remaining slots are already zero-initialized by default construction
	/// No need to explicitly pad with empty LightData

	spdlog::trace("Prepared GPU data for {} active lights",
		ubo.lightCount);

	return ubo;
}

} /// namespace lillugsi::rendering