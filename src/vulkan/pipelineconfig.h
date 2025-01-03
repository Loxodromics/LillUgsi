#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <functional>

#include "vulkanwrappers.h"

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
/// This class manages both the configuration data and the resources needed for pipeline creation
///
/// Lifecycle and Resource Management:
/// - PipelineConfig is created by Material classes to specify their pipeline requirements
/// - All Vulkan resources (like shader modules) are managed through RAII handles
/// - The configuration and its resources remain valid until the PipelineConfig is destroyed
/// - No explicit cleanup is needed due to RAII design
///
/// Usage Flow:
/// 1. Material creates configuration and adds shader stages
/// 2. PipelineManager uses configuration to create actual pipeline
/// 3. Configuration and resources are automatically cleaned up
///
/// Resource Dependencies:
/// - Shader modules are kept alive through shaderModules member
/// - Shader stage information references these modules
/// - All pointers in pipeline create info refer to member variables
/// - Member variables ensure all referenced data remains valid
///
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
	/// @param device
	/// @param renderPass The render pass this pipeline will be used with
	/// @param layout The pipeline layout to use
	/// @return The pipeline create info structure
	[[nodiscard]] VkGraphicsPipelineCreateInfo getCreateInfo(VkDevice device, VkRenderPass renderPass,
		VkPipelineLayout layout);

private:
	/// Shader stages configuration
	std::vector<PipelineShaderStage> shaderStages;

	/// Shader stage configuration
	/// References shader modules and must stay alive until pipeline creation
	std::vector<VkPipelineShaderStageCreateInfo> shaderStageInfos;

	/// Shader modules for pipeline creation
	/// These must stay alive until pipeline creation is complete
	/// as they are referenced by shaderStageInfos
	std::vector<VulkanShaderModuleHandle> shaderModules;

	/// Input assembly state
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};

	/// Vertex input state
	VkVertexInputBindingDescription vertexBindingDescription{};
	std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};

	/// Dynamic state for viewport and scissor
	/// These need to be dynamic for window resizing
	std::array<VkDynamicState, 2> dynamicStates;
	VkPipelineDynamicStateCreateInfo dynamicState{};

	/// Viewport and scissor state
	/// Even with dynamic viewport/scissor, we need to specify counts
	VkPipelineViewportStateCreateInfo viewportState{};

	/// Rasterization state
	VkPipelineRasterizationStateCreateInfo rasterization{};

	/// Depth-stencil state
	VkPipelineDepthStencilStateCreateInfo depthStencil{};

	/// Color blend state
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	VkPipelineColorBlendStateCreateInfo colorBlend{};

	/// Multisampling state
	/// We keep this as member to ensure pointer validity
	VkPipelineMultisampleStateCreateInfo multisampling{};

	/// Initialize all state structures with default values
	/// Called by constructor to ensure consistent initialization
	void initializeDefaults();
};

} /// namespace lillugsi::vulkan