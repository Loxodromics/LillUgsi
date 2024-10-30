#include "shadermodule.h"
#include <fstream>
#include <spdlog/spdlog.h>

namespace lillugsi::vulkan {

ShaderModule ShaderModule::fromSpirV(VkDevice device, const std::string& filepath, VkShaderStageFlagBits stage) {
	/// Read the shader file
	/// We use SPIR-V format as it's the format Vulkan directly understands
	/// This eliminates the need for runtime compilation
	auto code = readFile(filepath);

	/// Set up the shader module creation info
	/// The code size must be in bytes, but pCode expects a uint32_t pointer
	/// This is because SPIR-V is a 32-bit instruction set
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	/// Create the shader module
	/// We use VK_CHECK to ensure proper error handling
	VkShaderModule shaderModule;
	VK_CHECK(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule));

	/// Create a VulkanShaderModuleHandle for RAII management
	/// The lambda captures the device for use in cleanup
	auto moduleHandle = VulkanShaderModuleHandle(shaderModule, [device](VkShaderModule sm) {
		vkDestroyShaderModule(device, sm, nullptr);
	});

	spdlog::info("Created shader module from file: {}", filepath);

	/// Return a new ShaderModule instance
	/// The move constructor ensures efficient transfer of the module handle
	return ShaderModule(device, std::move(moduleHandle), stage);
}

ShaderModule::ShaderModule(VkDevice device, VulkanShaderModuleHandle module, VkShaderStageFlagBits stage)
	: device(device)
	, shaderModule(std::move(module))
	, stage(stage) {
}

VkPipelineShaderStageCreateInfo ShaderModule::getStageCreateInfo() const {
	/// Create and return the shader stage creation info
	/// This structure is used when creating a graphics pipeline
	/// We set the entry point to "main" as this is the conventional name
	/// used in most shaders
	VkPipelineShaderStageCreateInfo stageInfo{};
	stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageInfo.stage = this->stage;
	stageInfo.module = this->shaderModule.get();
	stageInfo.pName = "main"; /// The entry point of the shader

	return stageInfo;
}

std::vector<char> ShaderModule::readFile(const std::string& filepath) {
	/// Open the file at the end to get its size
	/// Using binary mode is crucial as SPIR-V is a binary format
	std::ifstream file(filepath, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		throw VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Failed to open shader file: " + filepath,
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	/// Get the file size and create a buffer
	size_t fileSize = static_cast<size_t>(file.tellg());
	std::vector<char> buffer(fileSize);

	/// Go back to the start of the file and read its contents
	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();

	spdlog::debug("Read shader file: {}, size: {} bytes", filepath, fileSize);

	return buffer;
}

} /// namespace lillugsi::vulkan