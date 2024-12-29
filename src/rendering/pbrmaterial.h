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
	/// Default shader paths for PBR material
	/// These static constants define the standard PBR shader locations
	/// which can be overridden in the constructor if needed
	static constexpr const char* DefaultVertexShaderPath = "shaders/pbr.vert.spv";
	static constexpr const char* DefaultFragmentShaderPath = "shaders/pbr.frag.spv";

	/// Create a new PBR material
	/// @param device The logical device for creating GPU resources
	/// @param name Unique name for this material instance
	/// @param physicalDevice The logical device for findMemoryType
	/// @param vertexShaderPath Optional path to custom vertex shader
	/// @param fragmentShaderPath Optional path to custom fragment shader
	PBRMaterial(
		VkDevice device,
		const std::string& name,
		VkPhysicalDevice physicalDevice,
		const std::string& vertexShaderPath = DefaultVertexShaderPath,
		const std::string& fragmentShaderPath = DefaultFragmentShaderPath
	);
	~PBRMaterial() override;

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

protected:
	/// GPU-aligned material properties structure
	/// We use this layout to match the shader's uniform buffer
	struct Properties {
		glm::vec4 baseColor{1.0f};  /// RGB + alpha
		float roughness{0.5f};           /// Default: medium roughness
		float metallic{0.0f};            /// Default: dielectric
		float ambient{1.0f};             /// Default: fully unoccluded
		float padding;                   /// Required for GPU alignment
	};

	/// Create the descriptor set layout for PBR materials
	/// This defines what resources the material shader can access
	void createDescriptorSetLayout();

	/// Create and initialize the uniform buffer
	/// This holds the material properties on the GPU
	void createUniformBuffer();

	/// Create and initialize the descriptor set
	/// This allocates the descriptor set from our pool and
	/// updates it with the uniform buffer information
	void createDescriptorSet();

	/// Update the uniform buffer with current properties
	/// Called whenever material properties change
	void updateUniformBuffer();

	Properties properties;   /// CPU-side material properties
};

} /// namespace lillugsi::rendering