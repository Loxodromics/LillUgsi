#pragma once

#include "material.h"
#include "pbrmaterial.h"
#include "custommaterial.h"
#include "wireframematerial.h"
#include "vulkan/vulkanwrappers.h"
#include <string>
#include <memory>
#include <unordered_map>

namespace lillugsi::rendering {

/// MaterialManager centralizes material creation and lifecycle management
/// We use a central manager to:
/// 1. Ensure consistent material naming and lookup
/// 2. Enable material reuse through caching
/// 3. Manage GPU resource lifecycle
/// 4. Provide a single point of control for material system features
class MaterialManager {
public:
	/// Create the material manager
	/// @param device Logical device for creating GPU resources
	/// @param physicalDevice Physical device for memory allocation
	MaterialManager(VkDevice device, VkPhysicalDevice physicalDevice);
	~MaterialManager();

	/// Prevent copying to ensure single ownership of GPU resources
	MaterialManager(const MaterialManager&) = delete;
	MaterialManager& operator=(const MaterialManager&) = delete;

	/// Create a new PBR material
	/// If a material with the given name already exists, it will be returned
	/// @param name Unique identifier for the material
	/// @return Shared pointer to the created or existing material
	[[nodiscard]] std::shared_ptr<PBRMaterial> createPBRMaterial(const std::string& name);

	/// Create a new custom material with specified shaders
	/// @param name Unique identifier for the material
	/// @param vertexShaderPath Path to vertex shader SPIR-V file
	/// @param fragmentShaderPath Path to fragment shader SPIR-V file
	/// @return Shared pointer to the created material
	/// @throws VulkanException if a material with the name exists
	[[nodiscard]] std::shared_ptr<CustomMaterial> createCustomMaterial(
		const std::string& name,
		const std::string& vertexShaderPath,
		const std::string& fragmentShaderPath
	);

	/// Create a new Wireframe material
	/// If a material with the given name already exists, it will be returned
	/// @param name Unique identifier for the material
	/// @return Shared pointer to the created or existing material
	[[nodiscard]] std::shared_ptr<WireframeMaterial> createWireframeMaterial(const std::string& name);

	/// Get a material by name
	/// @param name The name of the material to retrieve
	/// @return Shared pointer to the material, or nullptr if not found
	[[nodiscard]] std::shared_ptr<Material> getMaterial(const std::string& name) const;

	/// Check if a material exists
	/// @param name The name to check
	/// @return true if the material exists
	[[nodiscard]] bool hasMaterial(const std::string& name) const;

	/// Get all managed materials
	/// This can be useful for batch operations or debugging
	/// @return Const reference to the material map
	[[nodiscard]] const std::unordered_map<std::string, std::shared_ptr<Material>>&
	getMaterials() const { return this->materials; }

	/// Clean up all materials
	/// This should be called before the Vulkan device is destroyed
	void cleanup();

private:
	/// Validate material name and check for duplicates
	/// @param name The name to validate
	/// @throws VulkanException if name is invalid or exists
	void validateMaterialName(const std::string& name) const;

	VkDevice device;
	VkPhysicalDevice physicalDevice;
	std::unordered_map<std::string, std::shared_ptr<Material>> materials;
};

} /// namespace lillugsi::rendering