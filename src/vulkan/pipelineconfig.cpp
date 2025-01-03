#include "pipelineconfig.h"
#include "vulkan/vulkanexception.h"
#include "vulkan/vulkanwrappers.h"
#include "vulkan/vulkanformatters.h"
#include <spdlog/spdlog.h>
#include <functional>
#include <vulkan/shadermodule.h>

namespace lillugsi::vulkan {

PipelineConfig::PipelineConfig() {
	/// Initialize all state structures with sensible defaults
	/// This ensures a valid starting configuration and prevents undefined behavior
	this->initializeDefaults();
	spdlog::debug("Created pipeline configuration with default settings");
}

void PipelineConfig::initializeDefaults() {
	/// Initialize dynamic states first as they affect other settings
	/// We always want viewport and scissor to be dynamic for window resizing
	this->dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	this->dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	this->dynamicState.dynamicStateCount = static_cast<uint32_t>(this->dynamicStates.size());
	this->dynamicState.pDynamicStates = this->dynamicStates.data();

	/// Initialize vertex input state
	this->vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	/// Initialize viewport state
	/// Count must match dynamic state configuration
	this->viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	this->viewportState.viewportCount = 1;
	this->viewportState.scissorCount = 1;

	/// Initialize multisampling with default settings
	this->multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	this->multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	/// Set up input assembly defaults
	/// We use triangle lists as the most common primitive type
	this->inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	this->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	this->inputAssembly.primitiveRestartEnable = VK_FALSE;

	/// Set up rasterization defaults
	/// We use conservative defaults that work for most use cases:
	/// - Fill mode for normal rendering
	/// - Back-face culling for performance
	/// - Counter-clockwise front face (standard OpenGL convention)
	this->rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	this->rasterization.depthClampEnable = VK_FALSE;
	this->rasterization.rasterizerDiscardEnable = VK_FALSE;
	this->rasterization.polygonMode = VK_POLYGON_MODE_FILL;
	this->rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
	this->rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	this->rasterization.depthBiasEnable = VK_FALSE;
	this->rasterization.lineWidth = 1.0f;

	/// Set up depth-stencil defaults
	/// We enable depth testing by default for 3D rendering
	/// Using GREATER for Reverse-Z depth buffer
	this->depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	this->depthStencil.depthTestEnable = VK_TRUE;
	this->depthStencil.depthWriteEnable = VK_TRUE;
	this->depthStencil.depthCompareOp = VK_COMPARE_OP_GREATER;
	this->depthStencil.depthBoundsTestEnable = VK_FALSE;
	this->depthStencil.stencilTestEnable = VK_FALSE;

	/// Set up color blend defaults
	/// We start with blending disabled, which is the most common case
	/// All color channels are enabled for writing
	this->colorBlendAttachment.blendEnable = VK_FALSE;
	this->colorBlendAttachment.colorWriteMask = 
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	this->colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	this->colorBlend.logicOpEnable = VK_FALSE;
	this->colorBlend.attachmentCount = 1;
	this->colorBlend.pAttachments = &this->colorBlendAttachment;
}

void PipelineConfig::addShaderStage(VkShaderStageFlagBits stage,
	const std::string& shaderPath, const char* entryPoint) {
	/// Validate shader stage
	/// We check if the stage is a valid graphics pipeline stage
	if (stage != VK_SHADER_STAGE_VERTEX_BIT &&
		stage != VK_SHADER_STAGE_FRAGMENT_BIT &&
		stage != VK_SHADER_STAGE_GEOMETRY_BIT) {
		throw VulkanException(
			VK_ERROR_VALIDATION_FAILED_EXT,
			"Invalid shader stage specified",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	/// Add shader stage to configuration
	this->shaderStages.push_back({stage, shaderPath, entryPoint});
	spdlog::debug("Added shader stage {} with path: {}", stage, shaderPath);
}

void PipelineConfig::setVertexInput(
	const VkVertexInputBindingDescription& bindingDescription,
	const std::vector<VkVertexInputAttributeDescription>& attributeDescriptions) {
	/// Store vertex input configuration
	/// We copy the data to ensure we own it and it remains valid
	this->vertexBindingDescription = bindingDescription;
	this->vertexAttributeDescriptions = attributeDescriptions;

	spdlog::debug("Set vertex input with {} attributes", attributeDescriptions.size());
}

void PipelineConfig::setInputAssembly(VkPrimitiveTopology topology, bool primitiveRestart) {
	/// Update input assembly configuration
	this->inputAssembly.topology = topology;
	this->inputAssembly.primitiveRestartEnable = primitiveRestart ? VK_TRUE : VK_FALSE;

	spdlog::debug("Set input assembly topology: {}, primitive restart: {}",
		topology, primitiveRestart);
}

void PipelineConfig::setRasterization(VkPolygonMode polygonMode,
	VkCullModeFlags cullMode, VkFrontFace frontFace, float lineWidth) {
	/// Update rasterization state
	this->rasterization.polygonMode = polygonMode;
	this->rasterization.cullMode = cullMode;
	this->rasterization.frontFace = frontFace;
	this->rasterization.lineWidth = lineWidth;

	spdlog::debug("Set rasterization state - polygon mode: {}, cull mode: {}, front face: {}",
		polygonMode, cullMode, frontFace);
}

void PipelineConfig::setDepthState(bool enableDepthTest,
	bool enableDepthWrite, VkCompareOp compareOp) {
	/// Update depth state configuration
	this->depthStencil.depthTestEnable = enableDepthTest ? VK_TRUE : VK_FALSE;
	this->depthStencil.depthWriteEnable = enableDepthWrite ? VK_TRUE : VK_FALSE;
	this->depthStencil.depthCompareOp = compareOp;

	spdlog::debug("Set depth state - test: {}, write: {}, compare op: {}",
		enableDepthTest, enableDepthWrite, compareOp);
}

void PipelineConfig::setBlendState(bool enableBlending,
	VkBlendFactor srcColorBlendFactor,
	VkBlendFactor dstColorBlendFactor,
	VkBlendOp colorBlendOp,
	VkBlendFactor srcAlphaBlendFactor,
	VkBlendFactor dstAlphaBlendFactor,
	VkBlendOp alphaBlendOp) {
	/// Configure color blend attachment state
	this->colorBlendAttachment.blendEnable = enableBlending ? VK_TRUE : VK_FALSE;
	this->colorBlendAttachment.srcColorBlendFactor = srcColorBlendFactor;
	this->colorBlendAttachment.dstColorBlendFactor = dstColorBlendFactor;
	this->colorBlendAttachment.colorBlendOp = colorBlendOp;
	this->colorBlendAttachment.srcAlphaBlendFactor = srcAlphaBlendFactor;
	this->colorBlendAttachment.dstAlphaBlendFactor = dstAlphaBlendFactor;
	this->colorBlendAttachment.alphaBlendOp = alphaBlendOp;

	spdlog::debug("Set blend state - enabled: {}, color blend op: {}", enableBlending, colorBlendOp);
}

size_t PipelineConfig::hash() const {
	/// Generate a hash combining all pipeline state
	/// We use the FNV-1a hash algorithm for good distribution
	size_t hash = 0x811c9dc5;

	/// Hash shader stages
	for (const auto& stage : this->shaderStages) {
		hash ^= std::hash<int>{}(static_cast<int>(stage.stage));
		hash ^= std::hash<std::string>{}(stage.shaderPath);
		hash ^= std::hash<std::string>{}(stage.entryPoint);
	}

	/// Hash vertex input state
	hash ^= std::hash<uint32_t>{}(this->vertexBindingDescription.binding);
	hash ^= std::hash<uint32_t>{}(this->vertexBindingDescription.stride);

	/// Hash other pipeline states
	hash ^= std::hash<uint32_t>{}(static_cast<uint32_t>(this->inputAssembly.topology));
	hash ^= std::hash<uint32_t>{}(static_cast<uint32_t>(this->rasterization.polygonMode));
	hash ^= std::hash<uint32_t>{}(static_cast<uint32_t>(this->rasterization.cullMode));
	hash ^= std::hash<uint32_t>{}(static_cast<uint32_t>(this->depthStencil.depthCompareOp));
	hash ^= std::hash<uint32_t>{}(this->colorBlendAttachment.blendEnable);

	return hash;
}

VkGraphicsPipelineCreateInfo PipelineConfig::getCreateInfo(
	VkDevice device, VkRenderPass renderPass, VkPipelineLayout layout) {

	/// Update vertex input configuration
	/// We need to update this here because the descriptions might have changed
	this->vertexInputInfo.vertexBindingDescriptionCount = 1;
	this->vertexInputInfo.pVertexBindingDescriptions = &this->vertexBindingDescription;
	this->vertexInputInfo.vertexAttributeDescriptionCount =
		static_cast<uint32_t>(this->vertexAttributeDescriptions.size());
	this->vertexInputInfo.pVertexAttributeDescriptions = this->vertexAttributeDescriptions.data();

	/// Convert shader stages to Vulkan format
	this->shaderStageInfos.reserve(this->shaderStages.size());
	this->shaderModules.reserve(this->shaderStages.size());

	for (const auto& stage : this->shaderStages) {
		auto shaderCode = ShaderModule::readFile(stage.shaderPath);

		/// Create shader module
		VkShaderModuleCreateInfo moduleCreateInfo{};
		moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		moduleCreateInfo.codeSize = shaderCode.size();
		moduleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

		VkShaderModule shaderModule;
		VK_CHECK(vkCreateShaderModule(device, &moduleCreateInfo, nullptr, &shaderModule));

		/// Store module handle for cleanup
		this->shaderModules.emplace_back(shaderModule,
			[device](VkShaderModule sm) {
				vkDestroyShaderModule(device, sm, nullptr);
			});

		/// Set up shader stage info
		VkPipelineShaderStageCreateInfo shaderStageInfo{};
		shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageInfo.stage = stage.stage;
		shaderStageInfo.module = shaderModule;
		shaderStageInfo.pName = stage.entryPoint;
		this->shaderStageInfos.push_back(shaderStageInfo);

		spdlog::trace("Created shader stage for {}", stage.shaderPath);
	}

	/// Create dynamic state
	/// We always enable viewport and scissor as dynamic states
	/// This allows for window resizing without pipeline recreation
	static const std::array<VkDynamicState, 2> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	/// Set up vertex input state
	this->vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	this->vertexInputInfo.vertexBindingDescriptionCount = 1;
	this->vertexInputInfo.pVertexBindingDescriptions = &this->vertexBindingDescription;
	this->vertexInputInfo.vertexAttributeDescriptionCount =
		static_cast<uint32_t>(this->vertexAttributeDescriptions.size());
	this->vertexInputInfo.pVertexAttributeDescriptions = this->vertexAttributeDescriptions.data();

	/// Create viewport state
	/// We use dynamic viewport and scissor, so we only need to specify the count
	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	/// Set up multisample state
	/// We use single sample anti-aliasing by default
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	/// Create the final pipeline create info
	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = static_cast<uint32_t>(this->shaderStageInfos.size());
	pipelineInfo.pStages = this->shaderStageInfos.data();
	pipelineInfo.pVertexInputState = &this->vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &this->inputAssembly;
	pipelineInfo.pViewportState = &this->viewportState;
	pipelineInfo.pRasterizationState = &this->rasterization;
	pipelineInfo.pMultisampleState = &this->multisampling;
	pipelineInfo.pDepthStencilState = &this->depthStencil;
	pipelineInfo.pColorBlendState = &this->colorBlend;
	pipelineInfo.pDynamicState = &this->dynamicState;
	pipelineInfo.layout = layout;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;

	return pipelineInfo;
}

} /// namespace lillugsi::vulkan