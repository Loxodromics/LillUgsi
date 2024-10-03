#pragma once

#include "vulkanwrappers.h"
#include "vulkanexception.h"
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace lillugsi::vulkan {

/// PipelineManager class
/// Responsible for creating and managing graphics pipelines
class PipelineManager {
public:
	/// Constructor
	/// @param device The logical Vulkan device
	/// @param renderPass The render pass with which the pipelines will be compatible
	PipelineManager(VkDevice device, VkRenderPass renderPass);

	/// Destructor
	~PipelineManager() = default;

	/// Create a graphics pipeline
	/// @param name A unique name for the pipeline
	/// @param vertShaderPath Path to the vertex shader file
	/// @param fragShaderPath Path to the fragment shader file
	/// @param vertexBindingDescription Vertex binding description
	/// @param vertexAttributeDescriptions Vertex attribute descriptions
	/// @param topology The primitive topology to use
	/// @param width The width of the render area
	/// @param height The height of the render area
	/// @param descriptorSetLayout The descriptor set layout to use
	/// @return A shared pointer to the created pipeline handle
	std::shared_ptr<VulkanPipelineHandle> createGraphicsPipeline(
		const std::string& name,
		const std::string& vertShaderPath,
		const std::string& fragShaderPath,
		const VkVertexInputBindingDescription& vertexBindingDescription,
		const std::vector<VkVertexInputAttributeDescription>& vertexAttributeDescriptions,
		VkPrimitiveTopology topology,
		uint32_t width,
		uint32_t height,
		VkDescriptorSetLayout descriptorSetLayout
	);

	/// Get a pipeline by name
	/// @param name The name of the pipeline to retrieve
	/// @return A shared pointer to the requested pipeline handle, or nullptr if not found
	std::shared_ptr<VulkanPipelineHandle> getPipeline(const std::string& name) const;

	/// Get a pipeline layout by name
	/// @param name The name of the pipeline layout to retrieve
	/// @return A shared pointer to the requested pipeline layout handle, or nullptr if not found
	std::shared_ptr<VulkanPipelineLayoutHandle> getPipelineLayout(const std::string& name) const;

	/// Clean up all pipelines
	void cleanup();

private:
	/// Create a shader module from file
	/// @param shaderPath Path to the shader file
	/// @return A handle to the created shader module
	VulkanShaderModuleHandle createShaderModule(const std::string& shaderPath);

	/// Read a file into a vector of chars
	/// @param filename The name of the file to read
	/// @return A vector containing the file contents
	std::vector<char> readFile(const std::string& filename);

	VkDevice device;
	VkRenderPass renderPass;
	/// Store pipelines and pipeline layouts as shared pointers
	/// This allows for shared ownership while keeping VulkanHandles move-only
	std::unordered_map<std::string, std::shared_ptr<VulkanPipelineHandle>> pipelines;
	std::unordered_map<std::string, std::shared_ptr<VulkanPipelineLayoutHandle>> pipelineLayouts;
};

}