#include "shaderprogram.h"
#include <spdlog/spdlog.h>

namespace lillugsi::vulkan {

ShaderProgram::ShaderProgram(VkDevice device)
	: device(device) {
}

ShaderProgram ShaderProgram::createGraphicsProgram(
	VkDevice device,
	const std::string& vertexPath,
	const std::string& fragmentPath
) {
	/// Create a new shader program instance
	ShaderProgram program(device);

	try {
		/// Create the vertex shader module
		/// We use std::optional's emplace to construct the ShaderModule in place
		program.vertexShader.emplace(
			ShaderModule::fromSpirV(device, vertexPath, VK_SHADER_STAGE_VERTEX_BIT)
		);
		spdlog::info("Vertex shader loaded: {}", vertexPath);

		/// Create the fragment shader module
		program.fragmentShader.emplace(
			ShaderModule::fromSpirV(device, fragmentPath, VK_SHADER_STAGE_FRAGMENT_BIT)
		);
		spdlog::info("Fragment shader loaded: {}", fragmentPath);

	} catch (const VulkanException& e) {
		/// If shader creation fails, we log the error and rethrow
		/// The destructors will clean up any successfully created shaders
		spdlog::error("Failed to create shader program: {}", e.what());
		throw;
	}

	spdlog::info("Graphics shader program created successfully");
	return program;
}

std::vector<VkPipelineShaderStageCreateInfo> ShaderProgram::getShaderStages() const {
	/// Create a vector to hold the shader stages
	/// We reserve space for the maximum number of stages we expect
	std::vector<VkPipelineShaderStageCreateInfo> stages;
	stages.reserve(2);  /// Currently supporting vertex and fragment stages

	/// Add vertex shader stage if present
	if (this->vertexShader) {
		stages.push_back(this->vertexShader->getStageCreateInfo());
	}

	/// Add fragment shader stage if present
	if (this->fragmentShader) {
		stages.push_back(this->fragmentShader->getStageCreateInfo());
	}

	/// Validate that we have the required stages for a graphics program
	if (stages.size() != 2) {
		throw VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Graphics program must have both vertex and fragment shaders",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	return stages;
}

} /// namespace lillugsi::vulkan