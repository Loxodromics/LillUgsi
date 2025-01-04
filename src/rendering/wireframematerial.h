#pragma once

#include "material.h"
#include "vulkan/vulkanwrappers.h"
#include <glm/glm.hpp>

namespace lillugsi::rendering {

/// WireframeMaterial provides a specialized material for debug visualization
/// We use this material to render meshes in wireframe mode, which helps with:
/// - Debugging mesh topology and structure
/// - Visualizing geometric complexity
/// - Checking model deformation and animation
/// The material supports custom colors and integrates with the existing
/// material management system while providing specialized pipeline configuration
class WireframeMaterial : public Material {
public:
	/// Create a wireframe material with custom configuration
	/// @param device The logical device for creating GPU resources
	/// @param name Unique identifier for this material instance
	/// @param physicalDevice The physical device for memory allocation
	WireframeMaterial(
		VkDevice device,
		const std::string& name,
		VkPhysicalDevice physicalDevice
	);
	~WireframeMaterial() override;

	[[nodiscard]] ShaderPaths getShaderPaths() const override;

	/// Set the wireframe color
	/// @param color The RGB color for line rendering
	void setColor(const glm::vec3& color);

	/// Get the current wireframe color
	/// @return The current RGB color
	[[nodiscard]] glm::vec3 getColor() const { return this->properties.color; }

private:
	/// We use a simpler layout than PBR as we only need color data
	void createDescriptorSetLayout();

	/// Create and initialize the uniform buffer
	/// This holds the material properties accessible by the shader
	void createUniformBuffer();

	/// Create descriptor pool and set
	/// These connect our uniform buffer to the shader
	void createDescriptorSet();

	/// Update the uniform buffer with current properties
	/// Called whenever material properties change
	void updateUniformBuffer();

	/// GPU-aligned material properties
	/// This structure matches the layout expected by our shaders
	/// We keep it simple with just color information
	struct Properties {
		alignas(16) glm::vec3 color{1.0f}; /// Default to white
		float padding;                      /// Required for GPU alignment
	};

	Properties properties;                  /// Current material properties

	/// Shader paths stored for pipeline creation
	static constexpr const char* vertexShaderPath = "shaders/wireframe.vert.spv";
	static constexpr const char* fragmentShaderPath = "shaders/wireframe.frag.spv";
};

} /// namespace lillugsi::rendering