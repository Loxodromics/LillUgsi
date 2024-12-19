/// pipelinemanager.cpp
#include "pipelinemanager.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <glm/matrix.hpp>

namespace lillugsi::vulkan {

PipelineManager::PipelineManager(VkDevice device, VkRenderPass renderPass)
	: device(device)
	, renderPass(renderPass) {
}

std::shared_ptr<VulkanPipelineHandle> PipelineManager::createGraphicsPipeline(
	const std::string& name,
	ShaderProgram&& shaderProgram,
	const VkVertexInputBindingDescription& vertexBindingDescription,
	const std::vector<VkVertexInputAttributeDescription>& vertexAttributeDescriptions,
	VkPrimitiveTopology topology,
	uint32_t width,
	uint32_t height,
	VkDescriptorSetLayout descriptorSetLayout,
	bool enableDepthTest
) {
	/// Store the shader program
	/// We move the program into our storage to maintain ownership
	/// Using insert_or_assign allows us to potentially update existing programs
	this->shaderPrograms.insert_or_assign(name, std::move(shaderProgram));

	/// Get the shader stages from the program
	/// The ShaderProgram handles all the stage creation and validation
	auto shaderStages = this->shaderPrograms.at(name).getShaderStages();

	/// Set up vertex input state
	/// This describes the format of the vertex data that will be provided to the vertex shader
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &vertexBindingDescription;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributeDescriptions.size());
	vertexInputInfo.pVertexAttributeDescriptions = vertexAttributeDescriptions.data();

	/// Set up input assembly state
	/// This describes how to assemble vertices into primitives
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = topology;  // Use the provided topology (e.g., triangle list, line strip)
	inputAssembly.primitiveRestartEnable = VK_FALSE;  // Don't use primitive restart

	/// Set up viewport and scissor
	/// The viewport describes the region of the framebuffer that the output will be rendered to
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(width);
	viewport.height = static_cast<float>(height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	/// The scissor rectangle defines in which regions pixels will actually be stored
	/// Any pixels outside the scissor rectangles will be discarded
	VkRect2D scissor{};
	scissor.offset = {0, 0};
	scissor.extent = {width, height};

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	/// Set up rasterizer state
	/// The rasterizer takes the geometry shaped by the vertices and turns it into fragments
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;  /// Don't clamp fragments to near and far planes
	rasterizer.rasterizerDiscardEnable = VK_FALSE;  /// Don't discard all primitives before rasterization stage
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;  /// Fill the area of the polygon with fragments
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;  /// Cull back faces
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;  /// Specify vertex order for faces to be considered front-facing
	rasterizer.depthBiasEnable = VK_FALSE;  /// Don't use depth bias

	/// Set up multisampling state
	/// Multisampling is one of the ways to perform anti-aliasing
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;  /// No multisampling, use 1 sample per pixel

	/// Set up color blending
	/// This describes how to combine colors in the framebuffer
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;  /// No blending, overwrite existing color

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	/// Configure depth and stencil state for Reverse-Z
	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = enableDepthTest ? VK_TRUE : VK_FALSE;
	depthStencil.depthWriteEnable = enableDepthTest ? VK_TRUE : VK_FALSE;

	/// Use GREATER instead of LESS for Reverse-Z
	/// In Reverse-Z, larger depth values are closer to the camera
	/// This is the opposite of traditional depth testing
	depthStencil.depthCompareOp = VK_COMPARE_OP_GREATER;

	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.minDepthBounds = 0.0f; /// Not used when depthBoundsTestEnable is VK_FALSE
	depthStencil.maxDepthBounds = 1.0f; /// Not used when depthBoundsTestEnable is VK_FALSE
	depthStencil.stencilTestEnable = VK_FALSE;

	/// Define the push constant range for model matrix
	/// We use push constants for per-object transforms as they provide
	/// the fastest way to update frequently changing data
	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;  /// Only needed in vertex shader
	pushConstantRange.offset = 0;  /// Start at beginning of push constant block
	pushConstantRange.size = sizeof(glm::mat4);  /// Size of model matrix

	/// Set up pipeline layout
	/// This describes the resources that can be accessed by the pipeline
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;  /// We're using one descriptor set layout
	pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;  /// Point to the descriptor set layout
	pipelineLayoutInfo.pushConstantRangeCount = 1;  /// One range for model matrix
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

	/// Create the pipeline layout
	VkPipelineLayout pipelineLayout;
	VK_CHECK(vkCreatePipelineLayout(this->device, &pipelineLayoutInfo, nullptr, &pipelineLayout));

	/// Set up the graphics pipeline creation info
	/// This combines all the state we've defined above into a single create info structure
	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());  /// For now that always 2: Vertex and fragment shader stages
	pipelineInfo.pStages = shaderStages.data();
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.renderPass = this->renderPass;
	pipelineInfo.subpass = 0;  // Index of the subpass in the render pass where this pipeline will be used
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;  // Not deriving from an existing pipeline

	/// Create the graphics pipeline
	VkPipeline graphicsPipeline;
	VK_CHECK(vkCreateGraphicsPipelines(this->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline));

	/// Create and store the pipeline handle as a shared pointer
	auto pipelineHandle = std::make_shared<VulkanPipelineHandle>(graphicsPipeline, [this](VkPipeline p) {
		vkDestroyPipeline(this->device, p, nullptr);
	});
	this->pipelines[name] = pipelineHandle;

	/// Create and store the pipeline layout handle as a shared pointer
	auto layoutHandle = std::make_shared<VulkanPipelineLayoutHandle>(pipelineLayout, [this](VkPipelineLayout pl) {
		vkDestroyPipelineLayout(this->device, pl, nullptr);
	});
	this->pipelineLayouts[name] = layoutHandle;

	spdlog::info("Graphics pipeline '{}' created successfully with depth testing {}",
		name, enableDepthTest ? "enabled" : "disabled");

	return pipelineHandle;
}

std::shared_ptr<VulkanPipelineHandle> PipelineManager::getPipeline(const std::string& name) const {
	auto it = this->pipelines.find(name);
	if (it != this->pipelines.end()) {
		return it->second;
	}
	spdlog::warn("Pipeline '{}' not found", name);
	return nullptr;
}

std::shared_ptr<VulkanPipelineLayoutHandle> PipelineManager::getPipelineLayout(const std::string& name) const {
	auto it = this->pipelineLayouts.find(name);
	if (it != this->pipelineLayouts.end()) {
		return it->second;
	}
	spdlog::warn("Pipeline layout '{}' not found", name);
	return nullptr;
}

void PipelineManager::cleanup() {
	/// Clean up all pipelines and pipeline layouts
	this->pipelines.clear();
	this->pipelineLayouts.clear();

	/// Clean up shader programs
	this->shaderPrograms.clear();

	spdlog::info("All pipelines and pipeline layouts cleaned up");
}

}
