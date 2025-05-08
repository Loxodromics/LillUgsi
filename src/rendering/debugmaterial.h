#pragma once

#include "material.h"
#include "vulkan/vulkanwrappers.h"
#include <glm/glm.hpp>

namespace lillugsi::rendering {

/// DebugMaterial provides a simple material for debugging mesh rendering
/// We use this material to visualize various aspects of mesh geometry
/// This helps us diagnose rendering issues like incorrect normals or winding order
class DebugMaterial : public Material {
public:
	/// Visualization modes available in the debug material
	/// Each mode helps us visualize a different aspect of the mesh
	enum class VisualizationMode {
		VertexColors,   /// Display raw vertex colors (default)
		NormalColors,   /// Visualize normals as RGB colors
		WindingOrder    /// Display winding order (green=front, red=back)
	};

	/// Default shader paths for debug material
	/// We place debug shaders in their own directory to keep them separate from production shaders
	static constexpr const char* DefaultVertexShaderPath = "shaders/debug.vert.spv";
	static constexpr const char* DefaultFragmentShaderPath = "shaders/debug.frag.spv";

	/// Create a debug material with custom configuration
	/// @param device The logical device for creating GPU resources
	/// @param name Unique identifier for this material instance
	/// @param physicalDevice The physical device for memory allocation
	/// @param vertexShaderPath Optional path to custom vertex shader
	/// @param fragmentShaderPath Optional path to custom fragment shader
	DebugMaterial(
		VkDevice device,
		const std::string& name,
		VkPhysicalDevice physicalDevice,
		const std::string& vertexShaderPath = DefaultVertexShaderPath,
		const std::string& fragmentShaderPath = DefaultFragmentShaderPath
	);
	~DebugMaterial() override;

	[[nodiscard]] ShaderPaths getShaderPaths() const override;

	/// Set the visualization mode for debugging
	/// This controls what aspect of the mesh we're visualizing
	/// @param mode The visualization mode to use
	void setVisualizationMode(VisualizationMode mode);

	/// Get the current visualization mode
	/// @return The active visualization mode
	[[nodiscard]] VisualizationMode getVisualizationMode() const;

	/// Set a global color multiplier for the debug material
	/// This allows us to tint all vertices uniformly for testing
	/// @param color The RGB color multiplier
	void setColorMultiplier(const glm::vec3& color);

	/// Get the current color multiplier
	/// @return The current RGB color multiplier
	[[nodiscard]] glm::vec3 getColorMultiplier() const { return this->properties.colorMultiplier; }

private:
	/// We use a minimal layout since debug only needs color information
	void createDescriptorSetLayout();

	/// Create and initialize the uniform buffer
	/// This holds the simple debug properties accessible by the shader
	void createUniformBuffer();

	/// Create descriptor pool and set
	/// These connect our uniform buffer to the shader
	void createDescriptorSet();

	/// Update the uniform buffer with current properties
	/// Called whenever debug material properties change
	void updateUniformBuffer();

	/// GPU-aligned material properties
	/// We keep this simple for debugging purposes while supporting different visualization modes
	struct Properties {
		alignas(16) glm::vec3 colorMultiplier{1.0f}; /// Multiplier for vertex colors
		alignas(4) int visualizationMode{0};         /// Current visualization mode (matches enum)
	};

	Properties properties;                  /// Current material properties

	/// Shader paths stored for pipeline creation
	std::string vertexShaderPath;
	std::string fragmentShaderPath;
};

} /// namespace lillugsi::rendering