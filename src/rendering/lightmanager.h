#pragma once

#include "light.h"
#include "vulkan/vulkanexception.h"
#include <vector>
#include <memory>

namespace lillugsi::rendering {

/// LightManager handles the organization and management of scene lights
/// This class centralizes light management and prepares light data for GPU upload
/// We separate light management to:
/// 1. Centralize light-related operations
/// 2. Efficiently batch light data updates
/// 3. Prepare for future features like light culling
class LightManager {
public:
	/// Current maximum number of supported lights
	/// We use a fixed limit to simplify GPU buffer management
	/// This could be made dynamic in the future if needed
	static constexpr uint32_t MaxLights = 16;

	LightManager() = default;

	/// Delete copy operations to prevent accidental copying
	/// Light management should be centralized
	LightManager(const LightManager&) = delete;
	LightManager& operator=(const LightManager&) = delete;

	/// Add a light to the manager
	/// @param light The light to add
	/// @return Index of the added light for future reference
	/// @throws VulkanException if maximum light count is exceeded
	uint32_t addLight(std::shared_ptr<Light> light);

	/// Remove a light by its index
	/// @param index The index of the light to remove
	void removeLight(uint32_t index);

	/// Remove all lights from the manager
	void removeAllLights();

	/// Get a light by its index
	/// @param index The index of the light to retrieve
	/// @return Shared pointer to the requested light
	/// @throws VulkanException if index is invalid
	[[nodiscard]] std::shared_ptr<Light> getLight(uint32_t index) const;

	/// Get all lights
	/// @return Vector of all managed lights
	[[nodiscard]] const std::vector<std::shared_ptr<Light>>& getLights() const {
		return this->lights;
	}

	/// Get light count
	/// @return Number of currently managed lights
	[[nodiscard]] size_t getLightCount() const {
		return this->lights.size();
	}

	/// Get GPU data for all lights
	/// This prepares the light data in a format ready for GPU upload
	/// @return Vector of GPU-formatted light data
	[[nodiscard]] std::vector<LightData> getLightData() const;

	/// Check if adding another light is possible
	/// @return true if another light can be added
	[[nodiscard]] bool canAddLight() const {
		return this->lights.size() < MaxLights;
	}

private:
	/// Storage for managed lights
	/// We use shared_ptr to allow lights to be referenced elsewhere in the scene
	std::vector<std::shared_ptr<Light>> lights;
};

} /// namespace lillugsi::rendering