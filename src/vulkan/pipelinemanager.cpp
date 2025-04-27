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
	/// Get shader paths and configurations
	auto config = material.getPipelineConfig();

	/// Get or create pipeline using configuration
	auto cacheEntry = this->getOrCreatePipeline(config, material);

	return cacheEntry.pipeline;
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
	auto it = this->materialPipelines.find(name);
	if (it != this->materialPipelines.end()) {
		return it->second.pipeline;
	}
	
	/// Log only if we haven't warned about this material before
	if (this->missingPipelineWarnings.find(name) == this->missingPipelineWarnings.end()) {
		spdlog::warn("Pipeline '{}' not found, using fallback pipeline", name);
		this->missingPipelineWarnings.insert(name);
	}

	return nullptr;
	// return this->fallbackPipeline; /// TODO
}

std::shared_ptr<VulkanPipelineLayoutHandle> PipelineManager::getPipelineLayout(
	const std::string& name) const {
	auto it = this->materialPipelines.find(name);
	if (it != this->materialPipelines.end()) {
		return it->second.layout;
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

PipelineManager::MaterialPipeline PipelineManager::getOrCreatePipeline(
	PipelineConfig &config, const rendering::Material &material) {
	/// Calculate configuration hash
	/// This identifies materials that can share pipelines
	size_t configHash = config.hash();

	/// Check if we have an existing pipeline for this configuration
	auto &cacheEntry = this->pipelinesByConfig[configHash];
	//
	// /// Create new pipeline and layout if this is a new configuration
	// if (cacheEntry.referenceCount == 0) {

	if (this->pipelines.find(material.getName()) == this->pipelines.end()) {
		/// Create pipeline layout first
		VkPipelineLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

		/// Set up descriptor layouts in the order expected by shaders
		/// We need three layouts: camera (set=0), lighting (set=1), material (set=2)
		/// Order matches shader set bindings
		std::vector<VkDescriptorSetLayout> descriptorSetLayouts = {
			this->getCameraDescriptorLayout(), /// set = 0
			this->getLightDescriptorLayout(),  /// set = 1
			material.getDescriptorSetLayout()  /// set = 2 (material-specific)
		};
		layoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
		layoutInfo.pSetLayouts = descriptorSetLayouts.data();

		/// Configure push constant for model matrix
		VkPushConstantRange pushConstantRange{};
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(glm::mat4); /// Model matrix size
		layoutInfo.pushConstantRangeCount = 1;
		layoutInfo.pPushConstantRanges = &pushConstantRange;

		VK_CHECK(vkCreatePipelineLayout(this->device, &layoutInfo, nullptr, &cacheEntry.layout));

		/// Create the graphics pipeline
		auto createInfo = config.getCreateInfo(this->device, this->renderPass, cacheEntry.layout);

		VK_CHECK(vkCreateGraphicsPipelines(
			this->device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &cacheEntry.pipeline));

		spdlog::info("Created new pipeline configuration with hash {:#x}", configHash);
	} else {
		spdlog::debug(
			"Reusing pipeline configuration with hash {:#x} for material '{}'",
			configHash,
			material.getName());
	}

	/// Create RAII handles for this material
	/// These share the underlying Vulkan objects but provide safe cleanup
	MaterialPipeline materialPipeline;
	materialPipeline.pipeline = std::make_shared<VulkanPipelineHandle>(
		cacheEntry.pipeline, [this, configHash](VkPipeline p) {
			/// Only decrease reference count, actual destruction happens in PipelineManager cleanup
			auto &entry = this->pipelinesByConfig[configHash];
			entry.referenceCount--;
			spdlog::debug("Released pipeline reference, remaining refs: {}", entry.referenceCount);
		});

	materialPipeline.layout = std::make_shared<VulkanPipelineLayoutHandle>(
		cacheEntry.layout, [this, configHash](VkPipelineLayout l) {
			/// Clean up layout when last reference is gone
			auto &entry = this->pipelinesByConfig[configHash];
			if (entry.referenceCount == 0) {
				vkDestroyPipelineLayout(this->device, l, nullptr);
			}
		});

	/// Increment reference count for this configuration
	cacheEntry.referenceCount++;

	/// Store material-specific handles
	this->materialPipelines[material.getName()] = materialPipeline;

	return materialPipeline;
}

bool PipelineManager::hasPipeline(const std::string& materialName) const {
	/// Check if we already have a cached pipeline for this material
	/// This is a simple lookup that doesn't trigger any Vulkan API calls
	// std::lock_guard<std::mutex> lock(this->pipelinesMutex);

	/// We need to check both the pipeline and pipeline layout maps
	/// Both need to exist for a complete pipeline
	auto pipelineIt = this->pipelines.find(materialName);
	auto layoutIt = this->pipelineLayouts.find(materialName);

	/// Only return true if both components exist
	/// A partial pipeline isn't usable and would cause rendering errors
	bool exists = (pipelineIt != this->pipelines.end() && layoutIt != this->pipelineLayouts.end());

	spdlog::trace("Pipeline for material '{}' {}",
		materialName, exists ? "exists" : "does not exist");

	return exists;
}

void PipelineManager::cleanup() {
	/// Clean up in reverse order of creation
	/// Clean up material-specific handles first
	this->materialPipelines.clear();

	/// Clean up shared pipeline resources
	for (const auto& [hash, cache] : this->pipelinesByConfig)
	{
		vkDestroyPipelineLayout(this->device, cache.layout, nullptr);
		vkDestroyPipeline(this->device, cache.pipeline, nullptr);
		spdlog::debug("Destroyed shared pipeline configuration {:#x}", hash);
	}
	this->pipelinesByConfig.clear();

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