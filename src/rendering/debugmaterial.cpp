#include "debugmaterial.h"
#include "vulkan/vulkanexception.h"
#include "vulkan/vulkanutils.h"
#include <spdlog/spdlog.h>

namespace lillugsi::rendering {

DebugMaterial::DebugMaterial(
	VkDevice device,
	const std::string& name,
	VkPhysicalDevice physicalDevice,
	const std::string& vertexShaderPath,
	const std::string& fragmentShaderPath)
	: Material(device, name, physicalDevice, MaterialType::Debug, MaterialFeatureFlags::None)
	, vertexShaderPath(vertexShaderPath)
	, fragmentShaderPath(fragmentShaderPath) {

	/// Create descriptor layout first as it's needed for other resources
	/// This establishes the interface between our material and shaders
	this->createDescriptorSetLayout();

	/// Create and initialize the uniform buffer for debug properties
	/// This provides GPU access to our debug visualization settings
	this->createUniformBuffer();

	/// Create descriptor pool and set
	/// These connect our uniform buffer to the shader pipeline
	this->createDescriptorPool();
	this->createDescriptorSet();

	spdlog::debug("Created debug material '{}' with default vertex color mode", this->name);
}

DebugMaterial::~DebugMaterial() {
	/// Clean up uniform buffer memory
	/// The base Material class handles other cleanup
	if (this->uniformBufferMemory != VK_NULL_HANDLE) {
		vkFreeMemory(this->device, this->uniformBufferMemory, nullptr);
	}

	spdlog::debug("Destroyed debug material '{}'", this->name);
}

ShaderPaths DebugMaterial::getShaderPaths() const {
	/// Return shader paths for debug rendering
	/// We use specialized shaders that focus on visualization
	ShaderPaths paths;
	paths.vertexPath = this->vertexShaderPath;
	paths.fragmentPath = this->fragmentShaderPath;

	/// Validate shader paths before returning
	if (!paths.isValid()) {
		throw vulkan::VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Invalid shader paths in debug material '" + this->name + "'",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	return paths;
}

void DebugMaterial::setVisualizationMode(VisualizationMode mode) {
	/// Convert enum to integer for shader uniform
	/// We use integer in shader to avoid dealing with enum compatibility
	this->properties.visualizationMode = static_cast<int>(mode);
	this->updateUniformBuffer();

	/// Log mode change for debugging
	const char* modeName;
	switch (mode) {
		case VisualizationMode::VertexColors: modeName = "VertexColors"; break;
		case VisualizationMode::NormalColors: modeName = "NormalColors"; break;
		case VisualizationMode::WindingOrder: modeName = "WindingOrder"; break;
		default: modeName = "Unknown";
	}

	spdlog::info("Set debug visualization mode to '{}' for material '{}'", modeName, this->name);
}

DebugMaterial::VisualizationMode DebugMaterial::getVisualizationMode() const {
	/// Convert stored integer back to enum for C++ interface
	return static_cast<VisualizationMode>(this->properties.visualizationMode);
}

void DebugMaterial::setColorMultiplier(const glm::vec3& color) {
	/// Update material color multiplier and sync with GPU
	this->properties.colorMultiplier = color;
	this->updateUniformBuffer();

	spdlog::trace("Set debug color multiplier to ({}, {}, {}) for material '{}'",
		color.r, color.g, color.b, this->name);
}

void DebugMaterial::createDescriptorSetLayout() {
	/// Create the descriptor layout for our uniform buffer
	/// We only need one binding for the debug properties
	VkDescriptorSetLayoutBinding binding{};
	binding.binding = 0;
	binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	binding.descriptorCount = 1;
	/// We need the properties in both vertex and fragment stages
	/// This allows us to use them for different visualization modes
	binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	binding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &binding;

	VkDescriptorSetLayout layout;
	VK_CHECK(vkCreateDescriptorSetLayout(this->device, &layoutInfo, nullptr, &layout));

	/// Wrap the layout in our RAII handle
	this->descriptorSetLayout = vulkan::VulkanDescriptorSetLayoutHandle(
		layout,
		[this](VkDescriptorSetLayout l) {
			vkDestroyDescriptorSetLayout(this->device, l, nullptr);
		});

	spdlog::debug("Created descriptor set layout for debug material '{}'", this->name);
}

void DebugMaterial::createUniformBuffer() {
	/// Create the uniform buffer for material properties
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = sizeof(Properties);
	bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkBuffer buffer;
	VK_CHECK(vkCreateBuffer(this->device, &bufferInfo, nullptr, &buffer));

	/// Wrap buffer in RAII handle
	this->uniformBuffer = vulkan::VulkanBufferHandle(
		buffer,
		[this](VkBuffer b) {
			vkDestroyBuffer(this->device, b, nullptr);
		});

	/// Get memory requirements and allocate
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(this->device, this->uniformBuffer.get(), &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = vulkan::utils::findMemoryType(
		this->physicalDevice,
		memRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	);

	VkDeviceMemory rawMemoryHandle;
	VK_CHECK(vkAllocateMemory(
		this->device,
		&allocInfo,
		nullptr,
		&rawMemoryHandle
	));

	/// Wrap in RAII handle
	this->uniformBufferMemory = vulkan::VulkanDeviceMemoryHandle(
		rawMemoryHandle,
		[this](VkDeviceMemory mem) {
			vkFreeMemory(this->device, mem, nullptr);
		}
	);

	VK_CHECK(vkBindBufferMemory(this->device, this->uniformBuffer.get(), this->uniformBufferMemory, 0));

	/// Initialize buffer with default properties
	this->updateUniformBuffer();

	spdlog::debug("Created uniform buffer for debug material '{}'", this->name);
}

void DebugMaterial::createDescriptorSet() {
	/// Allocate descriptor set from our pool
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = this->descriptorPool.get();
	allocInfo.descriptorSetCount = 1;
	const VkDescriptorSetLayout layout = this->descriptorSetLayout.get();
	allocInfo.pSetLayouts = &layout;

	VK_CHECK(vkAllocateDescriptorSets(this->device, &allocInfo, &this->descriptorSet));

	/// Update the descriptor set to point to our uniform buffer
	VkDescriptorBufferInfo bufferInfo{};
	bufferInfo.buffer = this->uniformBuffer.get();
	bufferInfo.offset = 0;
	bufferInfo.range = sizeof(Properties);

	VkWriteDescriptorSet descriptorWrite{};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = this->descriptorSet;
	descriptorWrite.dstBinding = 0;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pBufferInfo = &bufferInfo;

	vkUpdateDescriptorSets(this->device, 1, &descriptorWrite, 0, nullptr);

	spdlog::debug("Created descriptor set for debug material '{}'", this->name);
}

void DebugMaterial::updateUniformBuffer() {
	/// Map memory and update uniform buffer contents
	void* data;
	VK_CHECK(vkMapMemory(this->device, this->uniformBufferMemory, 0, sizeof(Properties), 0, &data));
	memcpy(data, &this->properties, sizeof(Properties));
	vkUnmapMemory(this->device, this->uniformBufferMemory);

	spdlog::trace("Updated uniform buffer for debug material '{}'", this->name);
}

} /// namespace lillugsi::rendering