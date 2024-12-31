#pragma once

#include "vulkan/vulkanwrappers.h"
#include "vulkan/vulkanexception.h"
#include <vulkan/vulkan.h>
#include <string>

namespace lillugsi::vulkan {

/// ShaderModule class encapsulates the loading and management of a single shader module
/// This class follows RAII principles and uses VulkanShaderModuleHandle for automatic cleanup
class ShaderModule {
public:
	/// Create a shader module from a SPIR-V file
	/// @param device The logical device to create the shader module on
	/// @param filepath Path to the SPIR-V shader file
	/// @param stage The shader stage this module represents
	/// @return A new ShaderModule instance
	static ShaderModule fromSpirV(VkDevice device, const std::string& filepath, VkShaderStageFlagBits stage);

	/// Default constructor deleted to prevent creation without proper initialization
	ShaderModule() = delete;

	/// Move constructor and assignment operator
	/// We allow moving but not copying to maintain RAII semantics
	ShaderModule(ShaderModule&& other) noexcept = default;
	ShaderModule& operator=(ShaderModule&& other) noexcept = default;

	/// Copy constructor and assignment operator deleted to maintain RAII semantics
	ShaderModule(const ShaderModule&) = delete;
	ShaderModule& operator=(const ShaderModule&) = delete;

	/// Get the shader module handle
	/// @return The VkShaderModule wrapped in a VulkanShaderModuleHandle
	const VulkanShaderModuleHandle& getHandle() const { return this->shaderModule; }

	/// Get the shader stage
	/// @return The shader stage flag bits
	VkShaderStageFlagBits getStage() const { return this->stage; }

	/// Get the shader stage creation info
	/// This method prepares the VkPipelineShaderStageCreateInfo structure needed for pipeline creation
	/// @return The shader stage creation info structure
	VkPipelineShaderStageCreateInfo getStageCreateInfo() const;

	/// Read a binary file into a vector of chars
	/// Helper method to load SPIR-V shader code from file
	/// @param filepath Path to the file to read
	/// @return Vector containing the file contents
	static std::vector<char> readFile(const std::string& filepath);

private:
	/// Constructor is private to enforce creation through factory methods
	ShaderModule(VkDevice device, VulkanShaderModuleHandle module, VkShaderStageFlagBits stage);

	/// The logical device associated with this shader module
	VkDevice device;

	/// The shader module handle wrapped in RAII container
	VulkanShaderModuleHandle shaderModule;

	/// The stage this shader operates in (vertex, fragment, etc.)
	VkShaderStageFlagBits stage;
};

} /// namespace lillugsi::vulkan