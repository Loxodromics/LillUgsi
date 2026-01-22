#include "snowmaterial.h"
#include "vulkan/vulkanexception.h"
#include "vulkan/vulkanutils.h"
#include <spdlog/spdlog.h>

namespace lillugsi::rendering {

SnowMaterial::SnowMaterial(
	VkDevice device,
	const std::string& name,
	VkPhysicalDevice physicalDevice)
	: Material(device, name, physicalDevice, MaterialType::Custom, MaterialFeatureFlags::None) {

	/// Create descriptor layout first as it's needed for other resources
	this->createDescriptorSetLayout();

	/// Create and initialize the uniform buffer for material properties
	this->createUniformBuffer();

	/// Create descriptor pool and set
	this->createDescriptorPool();
	this->createDescriptorSet();

	spdlog::debug("Created snow material '{}' with default properties", this->name);
}

SnowMaterial::~SnowMaterial() {
	/// RAII handles and base class handle all cleanup automatically
	spdlog::debug("Destroyed snow material '{}'", this->name);
}

ShaderPaths SnowMaterial::getShaderPaths() const {
	/// Return shader paths for snow rendering
	ShaderPaths paths;
	paths.vertexPath = vertexShaderPath;
	paths.fragmentPath = fragmentShaderPath;

	/// Validate shader paths before returning
	if (!paths.isValid()) {
		throw vulkan::VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Invalid shader paths in snow material '" + this->name + "'",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	return paths;
}

void SnowMaterial::setBaseAlbedo(const glm::vec4& albedo) {
	this->properties.baseAlbedo = albedo;
	this->updateUniformBuffer();
	spdlog::trace("Set base albedo for snow material '{}'", this->name);
}

void SnowMaterial::setDarkeningAmount(float amount) {
	this->properties.darkeningAmount = amount;
	this->updateUniformBuffer();
	spdlog::trace("Set darkening amount to {} for snow material '{}'", amount, this->name);
}

void SnowMaterial::setDarkeningPower(float power) {
	this->properties.darkeningPower = power;
	this->updateUniformBuffer();
	spdlog::trace("Set darkening power to {} for snow material '{}'", power, this->name);
}

void SnowMaterial::setFresnelPower(float power) {
	this->properties.fresnelPower = power;
	this->updateUniformBuffer();
	spdlog::trace("Set Fresnel power to {} for snow material '{}'", power, this->name);
}

void SnowMaterial::setRimIntensity(float intensity) {
	this->properties.rimIntensity = intensity;
	this->updateUniformBuffer();
	spdlog::trace("Set rim intensity to {} for snow material '{}'", intensity, this->name);
}

void SnowMaterial::setSkyColor(const glm::vec3& color) {
	this->properties.skyColor = color;
	this->updateUniformBuffer();
	spdlog::trace("Set sky color to ({}, {}, {}) for snow material '{}'",
		color.r, color.g, color.b, this->name);
}

void SnowMaterial::setSparkleScale(float scale) {
	this->properties.sparkleScale = scale;
	this->updateUniformBuffer();
	spdlog::trace("Set sparkle scale to {} for snow material '{}'", scale, this->name);
}

void SnowMaterial::setSparkleThreshold(float threshold) {
	this->properties.sparkleThreshold = threshold;
	this->updateUniformBuffer();
	spdlog::trace("Set sparkle threshold to {} for snow material '{}'", threshold, this->name);
}

void SnowMaterial::setSparkleIntensity(float intensity) {
	this->properties.sparkleIntensity = intensity;
	this->updateUniformBuffer();
	spdlog::trace("Set sparkle intensity to {} for snow material '{}'", intensity, this->name);
}

void SnowMaterial::setScatterColor(const glm::vec3& color) {
	this->properties.scatterColor = color;
	this->updateUniformBuffer();
	spdlog::trace("Set scatter color to ({}, {}, {}) for snow material '{}'",
		color.r, color.g, color.b, this->name);
}

void SnowMaterial::setCurvatureScale(float scale) {
	this->properties.curvatureScale = scale;
	this->updateUniformBuffer();
	spdlog::trace("Set curvature scale to {} for snow material '{}'", scale, this->name);
}

void SnowMaterial::setSSSLUT(std::shared_ptr<Texture> texture) {
	this->sssLUT = texture;
	this->updateTextureDescriptors();
	spdlog::info("Set SSS LUT texture for snow material '{}'", this->name);
}

void SnowMaterial::createDescriptorSetLayout() {
	/// Create the descriptor layout for our uniform buffer and SSS LUT sampler
	/// Binding 0: Uniform buffer for material properties
	/// Binding 1: SSS LUT sampler
	VkDescriptorSetLayoutBinding bindings[2];

	/// Binding 0: Uniform buffer
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[0].pImmutableSamplers = nullptr;

	/// Binding 1: SSS LUT sampler
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[1].pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 2;
	layoutInfo.pBindings = bindings;

	VkDescriptorSetLayout layout;
	VK_CHECK(vkCreateDescriptorSetLayout(this->device, &layoutInfo, nullptr, &layout));

	/// Wrap the layout in our RAII handle
	this->descriptorSetLayout = vulkan::VulkanDescriptorSetLayoutHandle(
		layout,
		[this](VkDescriptorSetLayout l) {
			vkDestroyDescriptorSetLayout(this->device, l, nullptr);
		});

	spdlog::debug("Created descriptor set layout for snow material '{}'", this->name);
}

bool SnowMaterial::createDescriptorPool() {
	/// We need:
	/// - 1 uniform buffer descriptor for material properties
	/// - 1 combined image sampler descriptor for SSS LUT texture
	constexpr uint32_t uniformBufferCount = 1;
	constexpr uint32_t samplerCount = 1;

	std::array<VkDescriptorPoolSize, 2> poolSizes{};

	/// Uniform buffer pool size
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = uniformBufferCount;

	/// Combined image sampler pool size for SSS LUT
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = samplerCount;

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = 1;  /// One descriptor set per material

	VkDescriptorPool pool;
	VkResult result = vkCreateDescriptorPool(this->device, &poolInfo, nullptr, &pool);
	if (result != VK_SUCCESS) {
		spdlog::error("Failed to create descriptor pool for snow material '{}'", this->name);
		return false;
	}

	/// Wrap in RAII handle
	this->descriptorPool = vulkan::VulkanDescriptorPoolHandle(
		pool,
		[this](VkDescriptorPool p) {
			vkDestroyDescriptorPool(this->device, p, nullptr);
		});

	spdlog::debug("Created descriptor pool for snow material '{}'", this->name);
	return true;
}

void SnowMaterial::createUniformBuffer() {
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
	/// Capture device by value to ensure it's valid when deleter runs during shutdown
	VkDevice device = this->device;
	this->uniformBufferMemory = vulkan::VulkanDeviceMemoryHandle(
		rawMemoryHandle,
		[device](VkDeviceMemory mem) {
			vkFreeMemory(device, mem, nullptr);
		}
	);

	VK_CHECK(vkBindBufferMemory(this->device, this->uniformBuffer.get(), this->uniformBufferMemory, 0));

	/// Initialize buffer with default properties
	this->updateUniformBuffer();

	spdlog::debug("Created uniform buffer for snow material '{}'", this->name);
}

void SnowMaterial::createDescriptorSet() {
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

	/// Bind the SSS LUT texture if present
	this->updateTextureDescriptors();

	spdlog::debug("Created descriptor set for snow material '{}'", this->name);
}

void SnowMaterial::updateUniformBuffer() {
	/// Map memory and update uniform buffer contents
	void* data;
	VK_CHECK(vkMapMemory(this->device, this->uniformBufferMemory, 0, sizeof(Properties), 0, &data));
	memcpy(data, &this->properties, sizeof(Properties));
	vkUnmapMemory(this->device, this->uniformBufferMemory);

	spdlog::trace("Updated uniform buffer for snow material '{}'", this->name);
}

void SnowMaterial::updateTextureDescriptors() {
	/// If we don't have an SSS LUT texture yet, skip updating
	if (!this->sssLUT) {
		return;
	}

	/// Update descriptor set binding 1 with SSS LUT texture
	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = this->sssLUT->getImageView();
	imageInfo.sampler = this->sssLUT->getSampler();

	VkWriteDescriptorSet descriptorWrite{};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = this->descriptorSet;
	descriptorWrite.dstBinding = 1;  /// SSS LUT is at binding 1
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(this->device, 1, &descriptorWrite, 0, nullptr);

	spdlog::debug("Updated SSS LUT texture descriptor for snow material '{}'", this->name);
}

void SnowMaterial::bind(VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout) const {
	/// Bind the descriptor set to set 2 (material set)
	/// Set 0 is camera uniforms, set 1 is light uniforms
	vkCmdBindDescriptorSets(
		cmdBuffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		pipelineLayout,
		2,  /// Material descriptor set
		1,
		&this->descriptorSet,
		0,
		nullptr
	);
}

} /// namespace lillugsi::rendering
