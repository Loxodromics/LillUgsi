#pragma once

#include "vulkanwrappers.h"
#include "vulkanexception.h"
#include "shaderprogram.h"
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
	/// @param shaderProgram The shader program containing vertex and fragment shaders
	/// @param vertexBindingDescription Vertex binding description
	/// @param vertexAttributeDescriptions Vertex attribute descriptions
	/// @param topology The primitive topology to use
	/// @param width The width of the render area
	/// @param height The height of the render area
	/// @param descriptorSetLayout The descriptor set layout to use
	/// @param enableDepthTest Whether to enable depth testing
	/// @return A shared pointer to the created pipeline handle
	std::shared_ptr<VulkanPipelineHandle> createGraphicsPipeline(
		const std::string& name,
		ShaderProgram&& shaderProgram,
		const VkVertexInputBindingDescription& vertexBindingDescription,
		const std::vector<VkVertexInputAttributeDescription>& vertexAttributeDescriptions,
		VkPrimitiveTopology topology,
		uint32_t width,
		uint32_t height,
		VkDescriptorSetLayout descriptorSetLayout,
		bool enableDepthTest
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
	VkDevice device;
	VkRenderPass renderPass;
	/// Store pipelines and pipeline layouts as shared pointers
	/// This allows for shared ownership while keeping VulkanHandles move-only
	std::unordered_map<std::string, std::shared_ptr<VulkanPipelineHandle>> pipelines;
	std::unordered_map<std::string, std::shared_ptr<VulkanPipelineLayoutHandle>> pipelineLayouts;

	/// Add storage for shader programs
	/// This ensures shader programs live as long as the pipeline
	std::unordered_map<std::string, ShaderProgram> shaderPrograms;
};

}