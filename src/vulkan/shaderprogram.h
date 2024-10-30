#pragma once

#include "vulkan/shadermodule.h"
#include <memory>
#include <optional>

namespace lillugsi::vulkan {

/// ShaderProgram represents a complete shader program consisting of multiple shader stages
/// This class manages the lifecycle of related shaders and provides a higher-level interface
/// for shader program creation and management
class ShaderProgram {
public:
	/// Create a graphics program from vertex and fragment shader files
	/// @param device The logical device to create the shaders on
	/// @param vertexPath Path to the vertex shader SPIR-V file
	/// @param fragmentPath Path to the fragment shader SPIR-V file
	/// @return A new ShaderProgram instance
	static ShaderProgram createGraphicsProgram(
		VkDevice device,
		const std::string& vertexPath,
		const std::string& fragmentPath
	);

	/// Move constructor and assignment operator
	/// We allow moving but not copying to maintain RAII semantics
	ShaderProgram(ShaderProgram&& other) noexcept = default;
	ShaderProgram& operator=(ShaderProgram&& other) noexcept = default;

	/// Copy constructor and assignment operator deleted to maintain RAII semantics
	ShaderProgram(const ShaderProgram&) = delete;
	ShaderProgram& operator=(const ShaderProgram&) = delete;

	/// Get all shader stages for pipeline creation
	/// @return Vector of shader stage create info structures
	std::vector<VkPipelineShaderStageCreateInfo> getShaderStages() const;

	/// Get the vertex shader module
	/// @return Optional reference to the vertex shader module
	const std::optional<ShaderModule>& getVertexShader() const { return this->vertexShader; }

	/// Get the fragment shader module
	/// @return Optional reference to the fragment shader module
	const std::optional<ShaderModule>& getFragmentShader() const { return this->fragmentShader; }

private:
	/// Private constructor to enforce creation through factory methods
	explicit ShaderProgram(VkDevice device);

	/// The logical device associated with this shader program
	VkDevice device;

	/// Optional shader modules for each stage
	/// We use std::optional because not all programs will use all stages
	std::optional<ShaderModule> vertexShader;
	std::optional<ShaderModule> fragmentShader;

	/// Future: Add compute shader support
	/// std::optional<ShaderModule> computeShader;
};

} /// namespace lillugsi::vulkan