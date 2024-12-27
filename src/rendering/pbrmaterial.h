#pragma once

#include "material.h"
#include "vulkan/vulkanwrappers.h"
#include <glm/glm.hpp>

namespace lillugsi::rendering {

/// PBRMaterial implements a basic physically-based rendering material
/// We use the metallic-roughness workflow as it's widely adopted and
/// provides good artistic control while maintaining physical accuracy
class PBRMaterial : public Material {
public:
	/// Create a new PBR material
	/// @param device The logical device for creating GPU resources
	/// @param name Unique name for this material instance
	/// @param physicalDevice The logical device for findMemoryType
	PBRMaterial(VkDevice device, const std::string& name, VkPhysicalDevice physicalDevice);
	~PBRMaterial() override;

	void bind(VkCommandBuffer cmdBuffer) const override;
	[[nodiscard]] VkDescriptorSetLayout getDescriptorSetLayout() const override;
	[[nodiscard]] std::string getName() const override { return this->name; }

	/// Set the base color of the material
	/// @param color RGB color with alpha
	void setBaseColor(const glm::vec4& color);

	/// Set the roughness value
	/// Higher values create a more diffuse appearance
	/// @param roughness Value between 0 (smooth) and 1 (rough)
	void setRoughness(float roughness);

	/// Set the metallic value
	/// Controls how metallic the surface appears
	/// @param metallic Value between 0 (dielectric) and 1 (metallic)
	void setMetallic(float metallic);

	/// Set the ambient occlusion value
	/// This approximates how much ambient light reaches the surface
	/// @param ambient Value between 0 (fully occluded) and 1 (unoccluded)
	void setAmbient(float ambient);

private:
	/// GPU-aligned material properties structure
	/// We use this layout to match the shader's uniform buffer
	struct Properties {
		glm::vec4 baseColor{1.0f};  /// RGB + alpha
		float roughness{0.5f};      /// Default: medium roughness
		float metallic{0.0f};       /// Default: dielectric
		float ambient{1.0f};        /// Default: fully unoccluded
		float padding;              /// Required for GPU alignment
	};

	/// Create the descriptor set layout for PBR materials
	/// This defines what resources the material shader can access
	void createDescriptorSetLayout();

	/// Create and initialize the uniform buffer
	/// This holds the material properties on the GPU
	void createUniformBuffer();

	/// Update the uniform buffer with current properties
	/// Called whenever material properties change
	void updateUniformBuffer();

	VkDevice device;                 /// Logical device reference
	VkPhysicalDevice physicalDevice; /// Physical device reference
	std::string name;                /// Unique material name
	Properties properties;           /// CPU-side material properties

	/// GPU resources
	vulkan::VulkanDescriptorSetLayoutHandle descriptorSetLayout;
	vulkan::VulkanBufferHandle uniformBuffer;
	VkDeviceMemory uniformBufferMemory;  /// Memory for uniform buffer
	VkDescriptorSet descriptorSet;       /// Descriptor set for binding
};

} /// namespace lillugsi::rendering