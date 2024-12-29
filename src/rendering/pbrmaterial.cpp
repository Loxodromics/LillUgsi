#include "pbrmaterial.h"
#include "vulkan/vulkanexception.h"
#include "vulkan/vulkanutils.h"
#include <spdlog/spdlog.h>

namespace lillugsi::rendering {

PBRMaterial::PBRMaterial(
	VkDevice device,
	const std::string& name,
	VkPhysicalDevice physicalDevice,
	const std::string& vertexShaderPath,
	const std::string& fragmentShaderPath )
	: Material(device, name, physicalDevice)
	/// Create shader program before descriptor layout
	/// as the layout depends on shader requirements
	, shaderProgram(vulkan::ShaderProgram::createGraphicsProgram(
		device,
		vertexShaderPath,
		fragmentShaderPath
	))
{
	/// Create descriptor layout first as it's needed for other resources
	this->createDescriptorSetLayout();

	/// Create and initialize the uniform buffer
	this->createUniformBuffer();

	/// Create descriptor pool and set
	this->createDescriptorPool();
	this->createDescriptorSet();

	spdlog::debug("Created PBR material '{}' with shaders: {} and {}",
		this->name, vertexShaderPath, fragmentShaderPath);
}

PBRMaterial::~PBRMaterial() {
	/// Clean up uniform buffer memory
	if (this->uniformBufferMemory != VK_NULL_HANDLE) {
		vkFreeMemory(this->device, this->uniformBufferMemory, nullptr);
	}

	spdlog::debug("Destroyed PBR material '{}'", this->name);
}

void PBRMaterial::createDescriptorSetLayout() {
	/// Define the binding for our material properties uniform buffer
	VkDescriptorSetLayoutBinding binding{};
	binding.binding = 0;
	binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	binding.descriptorCount = 1;
	binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	binding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &binding;

	VkDescriptorSetLayout layout;
	VK_CHECK(vkCreateDescriptorSetLayout(this->device, &layoutInfo, nullptr, &layout));

	/// Wrap in RAII handle for automatic cleanup
	this->descriptorSetLayout = vulkan::VulkanDescriptorSetLayoutHandle(
		layout,
		[this](VkDescriptorSetLayout l) {
			vkDestroyDescriptorSetLayout(this->device, l, nullptr);
		});

	spdlog::debug("Created descriptor set layout for PBR material '{}'", this->name);
}

void PBRMaterial::createUniformBuffer() {
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

	VK_CHECK(vkAllocateMemory(this->device, &allocInfo, nullptr, &this->uniformBufferMemory));
	VK_CHECK(vkBindBufferMemory(this->device, this->uniformBuffer.get(), this->uniformBufferMemory, 0));

	/// Initialize buffer with default properties
	this->updateUniformBuffer();

	spdlog::debug("Created uniform buffer for PBR material '{}'", this->name);
}

void PBRMaterial::createDescriptorSet() {
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = this->descriptorPool.get();
	allocInfo.descriptorSetCount = 1;
	const VkDescriptorSetLayout layout = this->descriptorSetLayout.get();
	allocInfo.pSetLayouts = &layout;

	VK_CHECK(vkAllocateDescriptorSets(this->device, &allocInfo, &this->descriptorSet));

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
}

void PBRMaterial::updateUniformBuffer() {
	void* data;
	VK_CHECK(vkMapMemory(this->device, this->uniformBufferMemory, 0, sizeof(Properties), 0, &data));
	memcpy(data, &this->properties, sizeof(Properties));
	vkUnmapMemory(this->device, this->uniformBufferMemory);
}

void PBRMaterial::setBaseColor(const glm::vec4& color) {
	this->properties.baseColor = color;
	this->updateUniformBuffer();
}

void PBRMaterial::setRoughness(float roughness) {
	this->properties.roughness = glm::clamp(roughness, 0.0f, 1.0f);
	this->updateUniformBuffer();
}

void PBRMaterial::setMetallic(float metallic) {
	this->properties.metallic = glm::clamp(metallic, 0.0f, 1.0f);
	this->updateUniformBuffer();
}

void PBRMaterial::setAmbient(float ambient) {
	this->properties.ambient = glm::clamp(ambient, 0.0f, 1.0f);
	this->updateUniformBuffer();
}

} /// namespace lillugsi::rendering