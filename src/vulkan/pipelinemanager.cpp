#include "pipelinemanager.h"
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

namespace lillugsi::vulkan {

PipelineManager::PipelineManager(VkDevice device, VkRenderPass renderPass)
	: device(device)
	, renderPass(renderPass) {
	spdlog::debug("Created pipeline manager");
}

void PipelineManager::initialize() {
	/// Create global descriptor layouts before any pipeline creation
	/// These layouts are required for all materials
	this->createGlobalDescriptorLayouts();
	spdlog::info("Pipeline manager initialized with global descriptor layouts");
}

std::shared_ptr<VulkanPipelineHandle> PipelineManager::createPipeline(
	const rendering::Material& material) {
	/// Get shader paths and create shader program
	auto shaderPaths = material.getShaderPaths();
	auto shaderProgram = this->getOrCreateShaderProgram(shaderPaths);
	if (!shaderProgram) {
		throw VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Failed to create shader program for material '" + material.getName() + "'",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	/// Get pipeline configuration from material
	auto config = material.getPipelineConfig();

	/// Create descriptor set layouts array based on material requirements
	/// We need three layouts: camera (set=0), lighting (set=1), material (set=2)
	/// Order matches shader set bindings
	std::vector<VkDescriptorSetLayout> descriptorSetLayouts = {
		this->getCameraDescriptorLayout(),    /// set = 0
		this->getLightDescriptorLayout(),     /// set = 1
		material.getDescriptorSetLayout()     /// set = 2
	};

	/// Create pipeline layout
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
	pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();

	/// Set up push constant range for model matrix
	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(glm::mat4);  /// Model matrix size

	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

	VkPipelineLayout pipelineLayout;
	VK_CHECK(vkCreatePipelineLayout(this->device, &pipelineLayoutInfo, nullptr, &pipelineLayout));

	/// Create pipeline layout handle for RAII
	auto layoutHandle = std::make_shared<VulkanPipelineLayoutHandle>(
		pipelineLayout,
		[this](VkPipelineLayout pl) {
			vkDestroyPipelineLayout(this->device, pl, nullptr);
		}
	);

	/// Get create info from config
	/// This includes all pipeline states configured by the material
	auto createInfo = config.getCreateInfo(this->device, this->renderPass, pipelineLayout);

	/// Create the graphics pipeline
	VkPipeline pipeline;
	VK_CHECK(vkCreateGraphicsPipelines(
		this->device,
		VK_NULL_HANDLE,  /// Optional pipeline cache
		1,
		&createInfo,
		nullptr,
		&pipeline
	));

	/// Create pipeline handle for RAII
	auto pipelineHandle = std::make_shared<VulkanPipelineHandle>(
		pipeline,
		[this](VkPipeline p) {
			vkDestroyPipeline(this->device, p, nullptr);
		}
	);

	/// Store both handles for later use
	/// We use material name as the key for consistency
	const auto& name = material.getName();
	this->pipelines[name] = pipelineHandle;
	this->pipelineLayouts[name] = layoutHandle;

	spdlog::info("Created pipeline and layout for material '{}'", name);

	return pipelineHandle;
}

std::shared_ptr<ShaderProgram> PipelineManager::createShaderProgram(
	const rendering::ShaderPaths& paths) {
	/// Create a new shader program from the given paths
	/// This creates actual Vulkan shader modules
	try {
		auto program = ShaderProgram::createGraphicsProgram(
			this->device,
			paths.vertexPath,
			paths.fragmentPath
		);

		spdlog::debug("Created shader program for vertex: {}, fragment: {}",
			paths.vertexPath, paths.fragmentPath);

		return program;
	}
	catch (const VulkanException& e) {
		spdlog::error("Failed to create shader program: {}", e.what());
		throw;
	}
}

std::shared_ptr<ShaderProgram> PipelineManager::getOrCreateShaderProgram(
	const rendering::ShaderPaths& paths) {
	/// Generate a unique key for these shader paths
	auto key = generateShaderKey(paths);

	/// Check if we already have a program for these shaders
	auto it = this->shaderPrograms.find(key);
	if (it != this->shaderPrograms.end()) {
		spdlog::trace("Reusing existing shader program for key: {}", key);
		return it->second;
	}

	/// Create new shader program if not found
	auto program = this->createShaderProgram(paths);
	this->shaderPrograms[key] = program;

	return program;
}

std::string PipelineManager::generateShaderKey(const rendering::ShaderPaths& paths) {
	/// Create a unique key by combining vertex and fragment paths
	/// We use a separator that's unlikely to appear in paths
	return paths.vertexPath + "||" + paths.fragmentPath;
}

std::shared_ptr<VulkanPipelineHandle> PipelineManager::getPipeline(
	const std::string& name) {
	auto it = this->pipelines.find(name);
	if (it != this->pipelines.end()) {
		return it->second;
	}
	
	/// Log only if we haven't warned about this material before
	if (this->missingPipelineWarnings.find(name) == this->missingPipelineWarnings.end()) {
		spdlog::warn("Pipeline '{}' not found, using fallback pipeline", name);
		this->missingPipelineWarnings.insert(name);
	}

	return nullptr;
	// return this->fallbackPipeline;
}

std::shared_ptr<VulkanPipelineLayoutHandle> PipelineManager::getPipelineLayout(
	const std::string& name) const {
	auto it = this->pipelineLayouts.find(name);
	if (it != this->pipelineLayouts.end()) {
		return it->second;
	}
	
	spdlog::warn("Pipeline layout '{}' not found", name);
	return nullptr;
}

void PipelineManager::createGlobalDescriptorLayouts() {
	/// Create camera descriptor set layout (set = 0)
	/// This layout describes the bindings for camera data
	{
		VkDescriptorSetLayoutBinding cameraBinding{};
		cameraBinding.binding = 0;
		cameraBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		cameraBinding.descriptorCount = 1;
		cameraBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		cameraBinding.pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings = &cameraBinding;

		VkDescriptorSetLayout layout;
		VK_CHECK(vkCreateDescriptorSetLayout(
			this->device,
			&layoutInfo,
			nullptr,
			&layout
		));

		this->cameraDescriptorLayout = VulkanDescriptorSetLayoutHandle(
			layout,
			[this](VkDescriptorSetLayout l) {
				vkDestroyDescriptorSetLayout(this->device, l, nullptr);
			}
		);

		spdlog::debug("Created camera descriptor set layout");
	}

	/// Create light descriptor set layout (set = 1)
	/// This layout describes the bindings for light data
	{
		VkDescriptorSetLayoutBinding lightBinding{};
		lightBinding.binding = 0;
		lightBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		lightBinding.descriptorCount = 1;
		lightBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		lightBinding.pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings = &lightBinding;

		VkDescriptorSetLayout layout;
		VK_CHECK(vkCreateDescriptorSetLayout(
			this->device,
			&layoutInfo,
			nullptr,
			&layout
		));

		this->lightDescriptorLayout = VulkanDescriptorSetLayoutHandle(
			layout,
			[this](VkDescriptorSetLayout l) {
				vkDestroyDescriptorSetLayout(this->device, l, nullptr);
			}
		);

		spdlog::debug("Created light descriptor set layout");
	}
}

void PipelineManager::cleanup() {
	/// Clean up in reverse order of creation
	this->pipelines.clear();
	this->pipelineLayouts.clear();
	this->shaderPrograms.clear();

	/// Clean up global descriptor layouts last
	this->lightDescriptorLayout.reset();
	this->cameraDescriptorLayout.reset();

	this->missingPipelineWarnings.clear();

	spdlog::info("Pipeline manager resources cleaned up");
}

} /// namespace lillugsi::vulkan