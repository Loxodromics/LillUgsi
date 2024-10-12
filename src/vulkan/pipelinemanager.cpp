/// pipelinemanager.cpp
#include "pipelinemanager.h"
#include <spdlog/spdlog.h>
#include <fstream>

namespace lillugsi::vulkan {

PipelineManager::PipelineManager(VkDevice device, VkRenderPass renderPass)
	: device(device)
	, renderPass(renderPass) {
}

std::shared_ptr<VulkanPipelineHandle> PipelineManager::createGraphicsPipeline(
	const std::string& name,
	const std::string& vertShaderPath,
	const std::string& fragShaderPath,
	const VkVertexInputBindingDescription& vertexBindingDescription,
	const std::vector<VkVertexInputAttributeDescription>& vertexAttributeDescriptions,
	VkPrimitiveTopology topology,
	uint32_t width,
	uint32_t height,
	VkDescriptorSetLayout descriptorSetLayout,
	bool enableDepthTest
) {
	/// Create shader modules from the provided shader paths
	/// Shader modules contain the compiled SPIR-V code for the shaders
	auto vertShaderModule = this->createShaderModule(vertShaderPath);
	auto fragShaderModule = this->createShaderModule(fragShaderPath);

	/// Set up shader stage creation information
	/// This describes how the shader modules should be used in the pipeline
	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;  // This is a vertex shader
	vertShaderStageInfo.module = vertShaderModule.get();
	vertShaderStageInfo.pName = "main";  // The entry point of the shader

	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;  // This is a fragment shader
	fragShaderStageInfo.module = fragShaderModule.get();
	fragShaderStageInfo.pName = "main";  // The entry point of the shader

	/// Combine both shader stages into an array
	/// This array will be used when creating the pipeline to specify all shader stages
	VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

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
	rasterizer.depthClampEnable = VK_FALSE;  // Don't clamp fragments to near and far planes
	rasterizer.rasterizerDiscardEnable = VK_FALSE;  // Don't discard all primitives before rasterization stage
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;  // Fill the area of the polygon with fragments
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;  // Cull back faces
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;  // Specify vertex order for faces to be considered front-facing
	rasterizer.depthBiasEnable = VK_FALSE;  // Don't use depth bias

	/// Set up multisampling state
	/// Multisampling is one of the ways to perform anti-aliasing
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;  // No multisampling, use 1 sample per pixel

	/// Set up color blending
	/// This describes how to combine colors in the framebuffer
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;  // No blending, overwrite existing color

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	/// Configure depth and stencil state
	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = enableDepthTest ? VK_TRUE : VK_FALSE;
	depthStencil.depthWriteEnable = enableDepthTest ? VK_TRUE : VK_FALSE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;

	/// Set up pipeline layout
	/// This describes the resources that can be accessed by the pipeline
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;  /// We're using one descriptor set layout
	pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;  /// Point to the descriptor set layout
	/// No push constants for now
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	/// Create the pipeline layout
	VkPipelineLayout pipelineLayout;
	VK_CHECK(vkCreatePipelineLayout(this->device, &pipelineLayoutInfo, nullptr, &pipelineLayout));

	/// Set up the graphics pipeline creation info
	/// This combines all the state we've defined above into a single create info structure
	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;  // Vertex and fragment shader stages
	pipelineInfo.pStages = shaderStages;
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

	spdlog::info("All pipelines and pipeline layouts cleaned up");
}

VulkanShaderModuleHandle PipelineManager::createShaderModule(const std::string& shaderPath) {
	auto code = this->readFile(shaderPath);

	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shaderModule;
	VK_CHECK(vkCreateShaderModule(this->device, &createInfo, nullptr, &shaderModule));

	return VulkanShaderModuleHandle(shaderModule, [this](VkShaderModule sm) {
		vkDestroyShaderModule(this->device, sm, nullptr);
	});
}

std::vector<char> PipelineManager::readFile(const std::string& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		throw VulkanException(VK_ERROR_INITIALIZATION_FAILED, "Failed to open file: " + filename, __FUNCTION__, __FILE__, __LINE__);
	}

	size_t fileSize = (size_t) file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();

	return buffer;
}

}