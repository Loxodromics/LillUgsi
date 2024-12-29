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
	/// @param descriptorSetLayouts Vector of descriptor set layouts in shader binding order
	/// @param enableDepthTest Whether to enable depth testing
	/// @return A shared pointer to the created pipeline handle
	[[nodiscard]] std::shared_ptr<VulkanPipelineHandle> createGraphicsPipeline(
		const std::string& name,
		ShaderProgram&& shaderProgram,
		const VkVertexInputBindingDescription& vertexBindingDescription,
		const std::vector<VkVertexInputAttributeDescription>& vertexAttributeDescriptions,
		VkPrimitiveTopology topology,
		uint32_t width,
		uint32_t height,
		const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts,
		bool enableDepthTest
	);

	/// Get a pipeline by name
	/// @param name The name of the pipeline to retrieve
	/// @return A shared pointer to the requested pipeline handle, or nullptr if not found
	[[nodiscard]] std::shared_ptr<VulkanPipelineHandle> getPipeline(const std::string& name) const;

	/// Get a pipeline layout by name
	/// @param name The name of the pipeline layout to retrieve
	/// @return A shared pointer to the requested pipeline layout handle, or nullptr if not found
	[[nodiscard]] std::shared_ptr<VulkanPipelineLayoutHandle> getPipelineLayout(const std::string& name) const;

	/// Clean up all pipelines
	void cleanup();

	/// Get the maximum size of push constants supported by the device
	/// This helps clients ensure they don't exceed hardware limits
	/// @return Maximum push constant size in bytes
	[[nodiscard]] static constexpr uint32_t getMaxPushConstantsSize() noexcept {
		return 128;  /// Vulkan minimum guaranteed size
	}

	/// Check if a push constant size is supported
	/// @param size Size to check in bytes
	/// @return true if the size is supported
	[[nodiscard]] static constexpr bool isPushConstantSizeSupported(
		uint32_t size) noexcept {
		return size <= getMaxPushConstantsSize();
	}

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