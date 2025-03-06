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
	const std::string& fragmentShaderPath)
	: Material(device, name, physicalDevice, MaterialType::PBR)
	, vertexShaderPath(vertexShaderPath)
	, fragmentShaderPath(fragmentShaderPath) {

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

ShaderPaths PBRMaterial::getShaderPaths() const {
	/// Return stored shader paths for pipeline creation
	/// These paths are validated during construction
	ShaderPaths paths;
	paths.vertexPath = this->vertexShaderPath;
	paths.fragmentPath = this->fragmentShaderPath;

	/// Ensure paths are still valid
	/// This check helps catch potential runtime issues
	if (!paths.isValid()) {
		throw vulkan::VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Invalid shader paths in PBR material '" + this->name + "'",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	return paths;
}

void PBRMaterial::createDescriptorSetLayout() {
	/// Create two bindings - one for the uniform buffer and one for the texture sampler
	std::array<VkDescriptorSetLayoutBinding, 2> bindings{};

	/// Binding 0: Uniform buffer containing material properties
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; /// Used in fragment shader
	bindings[0].pImmutableSamplers = nullptr;

	/// Binding 1: Combined image sampler for albedo texture
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[1].descriptorCount = 1; /// Just one texture for now
	bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; /// Used in fragment shader
	bindings[1].pImmutableSamplers = nullptr;

	/// Create the descriptor set layout with both bindings
	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	VkDescriptorSetLayout layout;
	VK_CHECK(vkCreateDescriptorSetLayout(this->device, &layoutInfo, nullptr, &layout));

	/// Wrap in RAII handle for automatic cleanup
	this->descriptorSetLayout = vulkan::VulkanDescriptorSetLayoutHandle(
		layout,
		[this](VkDescriptorSetLayout l) {
			vkDestroyDescriptorSetLayout(this->device, l, nullptr);
		}
	);

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

void PBRMaterial::setAlbedoTexture(std::shared_ptr<Texture> texture) {
	/// Store the new albedo texture
	this->albedoTexture = texture;

	/// Update the material property that controls texture usage
	/// We set this flag to 1.0 when a valid texture is provided
	/// and 0.0 when the texture is null, which controls shader behavior
	this->properties.useAlbedoTexture = (texture != nullptr) ? 1.0f : 0.0f;

	/// Update both the uniform buffer and descriptor sets
	/// The uniform buffer needs to reflect the new useAlbedoTexture value
	this->updateUniformBuffer();

	/// Textures are bound via descriptor sets, so we need to update those too
	this->updateTextureDescriptors();

	spdlog::debug("Albedo texture {} for material '{}'",
		texture ? "set" : "cleared", this->name);
}

void PBRMaterial::bind(VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout) const {
	/// First, invoke the base class implementation
	/// This handles binding the descriptor set containing uniform buffers
	Material::bind(cmdBuffer, pipelineLayout);

	/// No additional binding required here since we're using the same
	/// descriptor set (set=2) for both uniform data and textures.
	///
	/// The updateTextureDescriptors() method has already updated the
	/// descriptors to include both the uniform buffer and any textures.
	///
	/// When the descriptor set is bound by the base class method,
	/// it makes both the uniform data and textures available to the shader.
}

VkDescriptorSetLayout PBRMaterial::getDescriptorSetLayout() const {
	/// Return the descriptor set layout created in createDescriptorSetLayout()
	/// This layout now includes both uniform buffer and texture sampler bindings
	return this->descriptorSetLayout.get();
}

void PBRMaterial::updateTextureDescriptors() {
	/// Early out if we don't have a descriptor set yet
	/// This can happen during initialization before createDescriptorSet() is called
	if (this->descriptorSet == VK_NULL_HANDLE) {
		return;
	}

	/// Create an array of write operations for the descriptor set
	/// We need to update both the uniform buffer (binding 0) and texture sampler (binding 1)
	std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

	/// Configure the write operation for the uniform buffer
	/// This ensures the uniform data is updated along with textures
	VkDescriptorBufferInfo bufferInfo{};
	bufferInfo.buffer = this->uniformBuffer.get();
	bufferInfo.offset = 0;
	bufferInfo.range = sizeof(Properties);

	descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[0].dstSet = this->descriptorSet;
	descriptorWrites[0].dstBinding = 0;
	descriptorWrites[0].dstArrayElement = 0;
	descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorWrites[0].descriptorCount = 1;
	descriptorWrites[0].pBufferInfo = &bufferInfo;

	/// Configure the write operation for the texture sampler
	VkDescriptorImageInfo imageInfo{};

	/// If we have a valid albedo texture, use its image view and sampler
	/// Otherwise, use a default texture or null descriptor
	if (this->albedoTexture) {
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = this->albedoTexture->getImageView();
		imageInfo.sampler = this->albedoTexture->getSampler();
	} else {
		/// If no texture is set, we still need to provide valid data
		/// This prevents validation errors in the shader even if the texture won't be used
		/// In a real engine, you might have a default "white" texture for this purpose
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		/// Ideally, we'd get these from a default texture
		/// For now, we need to provide valid handles or skip this write
		if (this->properties.useAlbedoTexture < 0.5f) {
			/// Only perform one write if we're not using a texture
			vkUpdateDescriptorSets(
				this->device,
				1, /// Only update the uniform buffer
				descriptorWrites.data(),
				0, nullptr
			);
			return;
		} else {
			spdlog::warn("Material '{}' has useAlbedoTexture set but no texture provided", this->name);
		}
	}

	/// Set up the texture descriptor write
	descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[1].dstSet = this->descriptorSet;
	descriptorWrites[1].dstBinding = 1; /// Binding 1 is for the albedo texture
	descriptorWrites[1].dstArrayElement = 0;
	descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrites[1].descriptorCount = 1;
	descriptorWrites[1].pImageInfo = &imageInfo;

	/// Update the descriptor set with both the uniform buffer and texture
	vkUpdateDescriptorSets(
		this->device,
		this->albedoTexture ? 2 : 1, /// Update both descriptors if texture exists
		descriptorWrites.data(),
		0, nullptr
	);

	spdlog::trace("Updated texture descriptors for material '{}'", this->name);
}

} /// namespace lillugsi::rendering