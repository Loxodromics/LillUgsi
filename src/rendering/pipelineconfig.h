#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <functional>

namespace lillugsi::vulkan {

/// PipelineShaderStage encapsulates configuration for a single shader stage
/// We separate this into its own structure to make shader stage management more explicit
/// and to allow for easier addition of shader specialization in the future
struct PipelineShaderStage {
	VkShaderStageFlagBits stage;
	std::string shaderPath;
	const char* entryPoint = "main";
};

/// PipelineConfig represents the complete configuration needed to create a graphics pipeline
/// We use this structure to:
/// 1. Uniquely identify pipeline configurations for caching
/// 2. Encapsulate all pipeline creation parameters
/// 3. Enable efficient pipeline state comparison and hashing
class PipelineConfig {
public:
	/// Create a new pipeline configuration with default settings
	PipelineConfig();

	/// Add a shader stage to the pipeline
	/// @param stage The type of shader stage (vertex, fragment, etc.)
	/// @param shaderPath Path to the shader file
	/// @param entryPoint Name of the entry point function
	void addShaderStage(VkShaderStageFlagBits stage, const std::string& shaderPath,
		const char* entryPoint = "main");

	/// Set vertex input state
	/// @param bindingDescription Description of vertex buffer binding
	/// @param attributeDescriptions Descriptions of vertex attributes
	void setVertexInput(const VkVertexInputBindingDescription& bindingDescription,
		const std::vector<VkVertexInputAttributeDescription>& attributeDescriptions);

	/// Set input assembly state
	/// @param topology The primitive topology to use
	/// @param primitiveRestart Whether to enable primitive restart
	void setInputAssembly(VkPrimitiveTopology topology,
		bool primitiveRestart = false);

	/// Set rasterization state
	/// @param polygonMode Fill mode for polygons
	/// @param cullMode Face culling mode
	/// @param frontFace Front face vertex order
	/// @param lineWidth Width of lines when drawing lines
	void setRasterization(VkPolygonMode polygonMode,
		VkCullModeFlags cullMode,
		VkFrontFace frontFace,
		float lineWidth = 1.0f);

	/// Set depth state
	/// @param enableDepthTest Whether to enable depth testing
	/// @param enableDepthWrite Whether to enable depth writing
	/// @param compareOp Depth comparison operation
	void setDepthState(bool enableDepthTest,
		bool enableDepthWrite,
		VkCompareOp compareOp);

	/// Set blend state
	/// @param enableBlending Whether to enable blending
	/// @param srcColorBlendFactor Source color blend factor
	/// @param dstColorBlendFactor Destination color blend factor
	/// @param colorBlendOp Color blend operation
	/// @param srcAlphaBlendFactor Source alpha blend factor
	/// @param dstAlphaBlendFactor Destination alpha blend factor
	/// @param alphaBlendOp Alpha blend operation
	void setBlendState(bool enableBlending,
		VkBlendFactor srcColorBlendFactor,
		VkBlendFactor dstColorBlendFactor,
		VkBlendOp colorBlendOp,
		VkBlendFactor srcAlphaBlendFactor,
		VkBlendFactor dstAlphaBlendFactor,
		VkBlendOp alphaBlendOp);

	/// Generate a hash value for this configuration
	/// This hash is used for pipeline caching and comparison
	/// @return A hash value uniquely identifying this configuration
	[[nodiscard]] size_t hash() const;

	/// Get the complete pipeline create info
	/// @param renderPass The render pass this pipeline will be used with
	/// @param layout The pipeline layout to use
	/// @return The pipeline create info structure
	[[nodiscard]] VkGraphicsPipelineCreateInfo getCreateInfo(VkDevice device, VkRenderPass renderPass,
		VkPipelineLayout layout) const;

private:
	/// Shader stages configuration
	std::vector<PipelineShaderStage> shaderStages;

	/// Vertex input state
	VkVertexInputBindingDescription vertexBindingDescription{};
	std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;

	/// Input assembly state
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};

	/// Rasterization state
	VkPipelineRasterizationStateCreateInfo rasterization{};

	/// Depth state
	VkPipelineDepthStencilStateCreateInfo depthStencil{};

	/// Color blend state
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	VkPipelineColorBlendStateCreateInfo colorBlend{};

	/// Initialize all state structures with default values
	/// Called by constructor to ensure consistent initialization
	void initializeDefaults();
};

} /// namespace lillugsi::vulkan