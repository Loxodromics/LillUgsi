#include "custommaterial.h"
#include "vulkan/vulkanutils.h"
#include <spdlog/spdlog.h>

namespace lillugsi::rendering {

CustomMaterial::CustomMaterial(
	VkDevice device,
	const std::string& name,
	VkPhysicalDevice physicalDevice,
	const std::string& vertexShaderPath,
	const std::string& fragmentShaderPath
)
	: Material(device, name, physicalDevice)
	, shaderProgram(vulkan::ShaderProgram::createGraphicsProgram(
		device,
		vertexShaderPath,
		fragmentShaderPath
	))
{
	spdlog::debug("Created CustomMaterial '{}' with shaders: {} and {}",
		this->name, vertexShaderPath, fragmentShaderPath);
}

CustomMaterial::~CustomMaterial() {
	/// Free memory for all uniform buffers
	for (const auto& [name, info] : this->uniformBuffers) {
		vkFreeMemory(this->device, info.memory, nullptr);
	}

	if (this->descriptorPool) {
		/// Descriptor sets are automatically freed with the pool
		vkDestroyDescriptorPool(this->device, this->descriptorPool, nullptr);
	}

	spdlog::debug("Destroyed CustomMaterial '{}'", this->name);
}

void CustomMaterial::defineUniformBuffer(
	const std::string& name,
	VkDeviceSize size,
	VkShaderStageFlags stages
) {
	/// Ensure this uniform hasn't already been defined
	if (this->uniformBuffers.find(name) != this->uniformBuffers.end()) {
		throw vulkan::VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Uniform buffer '" + name + "' already exists",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	/// Create the uniform buffer
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkBuffer buffer;
	VK_CHECK(vkCreateBuffer(this->device, &bufferInfo, nullptr, &buffer));

	/// Get memory requirements and allocate
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(this->device, buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = vulkan::utils::findMemoryType(
		this->physicalDevice,
		memRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	);

	VkDeviceMemory memory;
	VK_CHECK(vkAllocateMemory(this->device, &allocInfo, nullptr, &memory));
	VK_CHECK(vkBindBufferMemory(this->device, buffer, memory, 0));

	/// Store buffer info
	UniformBufferInfo info{
		vulkan::VulkanBufferHandle(buffer, [this](VkBuffer b) {
			vkDestroyBuffer(this->device, b, nullptr);
		}),
		memory,
		size,
		stages,
		this->nextBinding++
	};

	this->uniformBuffers[name] = std::move(info);

	/// Create descriptor set layout if this is our first uniform
	if (this->uniformBuffers.size() == 1) {
		this->createDescriptorSetLayout();
		this->createDescriptorSets();
	}

	spdlog::debug("Defined uniform buffer '{}' in material '{}' with size {}",
		name, this->name, size);
}

void CustomMaterial::updateUniformBuffer(
	const std::string& name,
	const void* data,
	VkDeviceSize size,
	VkDeviceSize offset
) {
	/// Validate the update parameters
	this->validateUniformUpdate(name, size, offset);

	const auto& info = this->uniformBuffers.at(name);

	/// Map memory and copy data
	void* mapped;
	VK_CHECK(vkMapMemory(this->device, info.memory, offset, size, 0, &mapped));
	memcpy(mapped, data, size);
	vkUnmapMemory(this->device, info.memory);

	spdlog::trace("Updated uniform buffer '{}' in material '{}'", name, this->name);
}

void CustomMaterial::createDescriptorSetLayout() {
	/// Create bindings for all uniform buffers
	std::vector<VkDescriptorSetLayoutBinding> bindings;
	bindings.reserve(this->uniformBuffers.size());

	for (const auto& [name, info] : this->uniformBuffers) {
		VkDescriptorSetLayoutBinding binding{};
		binding.binding = info.binding;
		binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		binding.descriptorCount = 1;
		binding.stageFlags = info.stages;
		binding.pImmutableSamplers = nullptr;
		bindings.push_back(binding);
	}

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	VkDescriptorSetLayout layout;
	VK_CHECK(vkCreateDescriptorSetLayout(this->device, &layoutInfo, nullptr, &layout));

	this->descriptorSetLayout = vulkan::VulkanDescriptorSetLayoutHandle(
		layout,
		[this](VkDescriptorSetLayout l) {
			vkDestroyDescriptorSetLayout(this->device, l, nullptr);
		}
	);

	spdlog::debug("Created descriptor set layout for material '{}' with {} bindings",
		this->name, bindings.size());
}

void CustomMaterial::createDescriptorSets() {
	/// Create a pool large enough for our descriptors
	std::vector<VkDescriptorPoolSize> poolSizes;
	poolSizes.reserve(this->uniformBuffers.size());

	for (const auto& [name, info] : this->uniformBuffers) {
		VkDescriptorPoolSize poolSize{};
		poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSize.descriptorCount = 1;
		poolSizes.push_back(poolSize);
	}

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = 1;  /// We only need one set for now

	VkDescriptorPool pool;
	VK_CHECK(vkCreateDescriptorPool(this->device, &poolInfo, nullptr, &pool));

	this->descriptorPool = vulkan::VulkanDescriptorPoolHandle(
		pool,
		[this](VkDescriptorPool p) {
			vkDestroyDescriptorPool(this->device, p, nullptr);
		}
	);

	/// Allocate descriptor set
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = this->descriptorPool;
	allocInfo.descriptorSetCount = 1;
	VkDescriptorSetLayout layout = this->descriptorSetLayout.get();
	allocInfo.pSetLayouts = &layout;

	VK_CHECK(vkAllocateDescriptorSets(this->device, &allocInfo, &this->descriptorSet));

	/// Update descriptor set with buffer info
	std::vector<VkWriteDescriptorSet> descriptorWrites;
	std::vector<VkDescriptorBufferInfo> bufferInfos;
	descriptorWrites.reserve(this->uniformBuffers.size());
	bufferInfos.reserve(this->uniformBuffers.size());

	for (const auto& [name, info] : this->uniformBuffers) {
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = info.buffer;
		bufferInfo.offset = 0;
		bufferInfo.range = info.size;
		bufferInfos.push_back(bufferInfo);

		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = this->descriptorSet;
		descriptorWrite.dstBinding = info.binding;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pBufferInfo = &bufferInfos.back();
		descriptorWrites.push_back(descriptorWrite);
	}

	vkUpdateDescriptorSets(
		this->device,
		static_cast<uint32_t>(descriptorWrites.size()),
		descriptorWrites.data(),
		0, nullptr
	);

	spdlog::debug("Created descriptor pool and sets for material '{}'", this->name);
}

void CustomMaterial::validateUniformUpdate(const std::string& name, const VkDeviceSize size, const VkDeviceSize offset) const {
	/// Ensure the uniform buffer exists
	auto it = this->uniformBuffers.find(name);
	if (it == this->uniformBuffers.end()) {
		throw vulkan::VulkanException(
			VK_ERROR_VALIDATION_FAILED_EXT,
			"Uniform buffer '" + name + "' not found",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	/// Validate size and offset
	const auto& info = it->second;
	if (offset + size > info.size) {
		throw vulkan::VulkanException(
			VK_ERROR_VALIDATION_FAILED_EXT,
			"Update exceeds uniform buffer size",
			__FUNCTION__, __FILE__, __LINE__
		);
	}
}

} /// namespace lillugsi::rendering