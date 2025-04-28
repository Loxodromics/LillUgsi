#include "pbrmaterial.h"
#include "vulkan/vulkanutils.h"
#include <spdlog/spdlog.h>

namespace lillugsi::rendering {

PBRMaterial::PBRMaterial(
	VkDevice device,
	const std::string& name,
	VkPhysicalDevice physicalDevice,
	const std::string& vertexShaderPath,
	const std::string& fragmentShaderPath
) : Material(device, name, physicalDevice, MaterialType::PBR,
			 MaterialFeatureFlags::None),
	vertexShaderPath(vertexShaderPath),
	fragmentShaderPath(fragmentShaderPath) {

	/// Initialize the descriptor set layout
	this->createDescriptorSetLayout();

	/// Create the descriptor pool
	/// This must be done before allocating descriptor sets
	if (!this->createDescriptorPool()) {
		throw vulkan::VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Failed to create descriptor pool for PBR material",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	/// Create the uniform buffer to hold material properties
	this->createUniformBuffer();

	/// Create the descriptor set with initial configuration
	/// Now the descriptor pool exists and this should work
	this->createDescriptorSet();

	spdlog::debug("Created PBR material '{}'", name);
}

PBRMaterial::~PBRMaterial() {
	/// Explicit cleanup is not needed here because the Material base class
	/// and smart pointers handle resource cleanup automatically
	spdlog::debug("Destroyed PBR material '{}'", this->name);
}

ShaderPaths PBRMaterial::getShaderPaths() const {
	/// Return the configured shader paths for this material
	/// These paths are needed by the pipeline manager during pipeline creation
	ShaderPaths paths;
	paths.vertexPath = this->vertexShaderPath;
	paths.fragmentPath = this->fragmentShaderPath;
	return paths;
}

void PBRMaterial::setBaseColor(const glm::vec4& color) {
	/// Update the base color property
	/// This color serves as the albedo for the material
	this->properties.baseColor = color;

	/// Update the uniform buffer to reflect the change
	/// This ensures the shader always has the latest values
	this->updateUniformBuffer();

	spdlog::trace("Set base color to ({}, {}, {}, {}) for material '{}'",
		color.r, color.g, color.b, color.a, this->name);
}

void PBRMaterial::setRoughness(float roughness) {
	/// Clamp roughness to valid range [0,1]
	/// This prevents invalid values that could cause visual artifacts
	roughness = glm::clamp(roughness, 0.0f, 1.0f);

	/// Update the roughness property
	this->properties.roughness = roughness;

	/// Update the uniform buffer to reflect the change
	this->updateUniformBuffer();

	spdlog::trace("Set roughness to {} for material '{}'", roughness, this->name);
}

void PBRMaterial::setMetallic(float metallic) {
	/// Clamp metallic to valid range [0,1]
	/// Values outside this range don't make physical sense in PBR
	metallic = glm::clamp(metallic, 0.0f, 1.0f);

	/// Update the metallic property
	this->properties.metallic = metallic;

	/// Update the uniform buffer to reflect the change
	this->updateUniformBuffer();

	spdlog::trace("Set metallic to {} for material '{}'", metallic, this->name);
}

void PBRMaterial::setAmbient(float ambient) {
	/// Clamp ambient to valid range [0,1]
	ambient = glm::clamp(ambient, 0.0f, 1.0f);

	/// Update the ambient property
	this->properties.ambient = ambient;

	/// Update the uniform buffer to reflect the change
	this->updateUniformBuffer();

	spdlog::trace("Set ambient to {} for material '{}'", ambient, this->name);
}

void PBRMaterial::setNormalStrength(float strength) {
	/// Clamp strength to valid range [0,1]
	/// 0 = no effect, 1 = full effect
	strength = glm::clamp(strength, 0.0f, 1.0f);

	/// Update the normal strength property
	this->properties.normalStrength = strength;

	/// Update the uniform buffer to reflect the change
	this->updateUniformBuffer();

	spdlog::trace("Set normal strength to {} for material '{}'", strength, this->name);
}

void PBRMaterial::setRoughnessStrength(float strength) {
	/// Clamp strength to valid range [0,1]
	strength = glm::clamp(strength, 0.0f, 1.0f);

	/// Update the roughness strength property
	this->properties.roughnessStrength = strength;

	/// Update the uniform buffer to reflect the change
	this->updateUniformBuffer();

	spdlog::trace("Set roughness strength to {} for material '{}'", strength, this->name);
}

void PBRMaterial::setMetallicStrength(float strength) {
	/// Clamp strength to valid range [0,1]
	strength = glm::clamp(strength, 0.0f, 1.0f);

	/// Update the metallic strength property
	this->properties.metallicStrength = strength;

	/// Update the uniform buffer to reflect the change
	this->updateUniformBuffer();

	spdlog::trace("Set metallic strength to {} for material '{}'", strength, this->name);
}

void PBRMaterial::setOcclusionStrength(float strength) {
	/// Clamp strength to valid range [0,1]
	strength = glm::clamp(strength, 0.0f, 1.0f);

	/// Update the occlusion strength property
	this->properties.occlusionStrength = strength;

	/// Update the uniform buffer to reflect the change
	this->updateUniformBuffer();

	spdlog::trace("Set occlusion strength to {} for material '{}'", strength, this->name);
}

void PBRMaterial::setAlbedoTexture(std::shared_ptr<Texture> texture) {
	/// Store the texture reference
	this->albedoTexture = texture;

	/// Update tracking flag based on whether a valid texture was provided
	this->hasAlbedoTexture = (texture != nullptr);

	/// Update the uniform property to tell the shader whether to use the texture
	this->properties.useAlbedoTexture = this->hasAlbedoTexture ? 1.0f : 0.0f;

	/// Update the descriptor set with the new texture
	/// This connects the texture to the correct binding point in the shader
	this->updateTextureDescriptors();

	/// Update the uniform buffer to reflect the change
	this->updateUniformBuffer();

	spdlog::debug("Set albedo texture for material '{}': {}",
		this->name,
		this->hasAlbedoTexture ? texture->getName() : "none");
}

void PBRMaterial::setNormalMap(std::shared_ptr<Texture> texture, float strength) {
	/// Store the texture reference
	this->normalMap = texture;

	/// Update tracking flag based on whether a valid texture was provided
	this->hasNormalMap = (texture != nullptr);

	/// Set the normal map strength, clamping to valid range
	this->properties.normalStrength = glm::clamp(strength, 0.0f, 1.0f);

	/// Update the uniform property to tell the shader whether to use the normal map
	this->properties.useNormalMap = this->hasNormalMap ? 1.0f : 0.0f;

	/// Update the descriptor set with the new texture
	this->updateTextureDescriptors();

	/// Update the uniform buffer to reflect the changes
	this->updateUniformBuffer();

	spdlog::debug("Set normal map for material '{}': {} (strength: {})",
		this->name,
		this->hasNormalMap ? texture->getName() : "none",
		this->properties.normalStrength);
}

void PBRMaterial::setRoughnessMap(std::shared_ptr<Texture> texture, float strength) {
	/// Clear any previously set combined maps that might include roughness
	/// This prevents conflicts between different texture sources for the same property
	if (texture) {
		this->roughnessMetallicMap = nullptr;
		this->hasRoughnessMetallicMap = false;
		this->ormMap = nullptr;
		this->hasOrmMap = false;
	}

	/// Store the texture reference
	this->roughnessMap = texture;

	/// Update tracking flag based on whether a valid texture was provided
	this->hasRoughnessMap = (texture != nullptr);

	/// Set the roughness map strength, clamping to valid range
	this->properties.roughnessStrength = glm::clamp(strength, 0.0f, 1.0f);

	/// Update the uniform property to tell the shader whether to use the roughness map
	this->properties.useRoughnessMap = this->hasRoughnessMap ? 1.0f : 0.0f;

	/// Update the descriptor set with the new texture
	this->updateTextureDescriptors();

	/// Update the uniform buffer to reflect the changes
	this->updateUniformBuffer();

	spdlog::debug("Set roughness map for material '{}': {} (strength: {})",
		this->name,
		this->hasRoughnessMap ? texture->getName() : "none",
		this->properties.roughnessStrength);
}

void PBRMaterial::setMetallicMap(std::shared_ptr<Texture> texture, float strength) {
	/// Clear any previously set combined maps that might include metallic
	/// This ensures consistent behavior by avoiding multiple texture sources
	if (texture) {
		this->roughnessMetallicMap = nullptr;
		this->hasRoughnessMetallicMap = false;
		this->ormMap = nullptr;
		this->hasOrmMap = false;
	}

	/// Store the texture reference
	this->metallicMap = texture;

	/// Update tracking flag based on whether a valid texture was provided
	this->hasMetallicMap = (texture != nullptr);

	/// Set the metallic map strength, clamping to valid range
	this->properties.metallicStrength = glm::clamp(strength, 0.0f, 1.0f);

	/// Update the uniform property to tell the shader whether to use the metallic map
	this->properties.useMetallicMap = this->hasMetallicMap ? 1.0f : 0.0f;

	/// Update the descriptor set with the new texture
	this->updateTextureDescriptors();

	/// Update the uniform buffer to reflect the changes
	this->updateUniformBuffer();

	spdlog::debug("Set metallic map for material '{}': {} (strength: {})",
		this->name,
		this->hasMetallicMap ? texture->getName() : "none",
		this->properties.metallicStrength);
}

void PBRMaterial::setOcclusionMap(std::shared_ptr<Texture> texture, float strength) {
	/// Clear any previously set combined maps that might include occlusion
	/// This ensures we don't have multiple sources of occlusion data
	if (texture) {
		this->ormMap = nullptr;
		this->hasOrmMap = false;
	}

	/// Store the texture reference
	this->occlusionMap = texture;

	/// Update tracking flag based on whether a valid texture was provided
	this->hasOcclusionMap = (texture != nullptr);

	/// Set the occlusion map strength, clamping to valid range
	this->properties.occlusionStrength = glm::clamp(strength, 0.0f, 1.0f);

	/// Update the uniform property to tell the shader whether to use the occlusion map
	this->properties.useOcclusionMap = this->hasOcclusionMap ? 1.0f : 0.0f;

	/// Update the descriptor set with the new texture
	this->updateTextureDescriptors();

	/// Update the uniform buffer to reflect the changes
	this->updateUniformBuffer();

	spdlog::debug("Set occlusion map for material '{}': {} (strength: {})",
		this->name,
		this->hasOcclusionMap ? texture->getName() : "none",
		this->properties.occlusionStrength);
}

void PBRMaterial::setRoughnessMetallicMap(
	std::shared_ptr<Texture> texture,
	TextureChannel roughChannel,
	TextureChannel metalChannel,
	float roughStrength,
	float metalStrength
) {
	/// Clear any individual maps that might conflict with this combined map
	/// This prevents inconsistent rendering due to multiple texture sources
	if (texture) {
		this->roughnessMap = nullptr;
		this->hasRoughnessMap = false;
		this->metallicMap = nullptr;
		this->hasMetallicMap = false;
		this->ormMap = nullptr;
		this->hasOrmMap = false;
	}

	/// Store the texture reference
	this->roughnessMetallicMap = texture;

	/// Update tracking flag based on whether a valid texture was provided
	this->hasRoughnessMetallicMap = (texture != nullptr);

	/// Set channel masks for shader sampling
	/// These tell the shader which channels to read from the texture
	this->properties.roughnessChannel = this->channelToMask(roughChannel);
	this->properties.metallicChannel = this->channelToMask(metalChannel);

	/// Set strength factors, clamping to valid ranges
	this->properties.roughnessStrength = glm::clamp(roughStrength, 0.0f, 1.0f);
	this->properties.metallicStrength = glm::clamp(metalStrength, 0.0f, 1.0f);

	/// Update the uniform properties to tell the shader to use the combined map
	this->properties.useRoughnessMap = this->hasRoughnessMetallicMap ? 1.0f : 0.0f;
	this->properties.useMetallicMap = this->hasRoughnessMetallicMap ? 1.0f : 0.0f;

	/// Update the descriptor set with the new texture
	this->updateTextureDescriptors();

	/// Update the uniform buffer to reflect the changes
	this->updateUniformBuffer();

	spdlog::debug("Set roughness-metallic map for material '{}': {} (R:{}/M:{})",
		this->name,
		this->hasRoughnessMetallicMap ? texture->getName() : "none",
		this->properties.roughnessStrength,
		this->properties.metallicStrength);
}

void PBRMaterial::setOcclusionRoughnessMetallicMap(
	std::shared_ptr<Texture> texture,
	TextureChannel occlusionChannel,
	TextureChannel roughnessChannel,
	TextureChannel metallicChannel,
	float occlusionStrength,
	float roughnessStrength,
	float metallicStrength
) {
	/// Clear any individual or partial combined maps that might conflict
	/// This ensures we have a single authoritative source for all three properties
	if (texture) {
		this->occlusionMap = nullptr;
		this->hasOcclusionMap = false;
		this->roughnessMap = nullptr;
		this->hasRoughnessMap = false;
		this->metallicMap = nullptr;
		this->hasMetallicMap = false;
		this->roughnessMetallicMap = nullptr;
		this->hasRoughnessMetallicMap = false;
	}

	/// Store the texture reference
	this->ormMap = texture;

	/// Update tracking flag based on whether a valid texture was provided
	this->hasOrmMap = (texture != nullptr);

	/// Set channel masks for shader sampling
	this->properties.occlusionChannel = this->channelToMask(occlusionChannel);
	this->properties.roughnessChannel = this->channelToMask(roughnessChannel);
	this->properties.metallicChannel = this->channelToMask(metallicChannel);

	/// Set strength factors, clamping to valid ranges
	this->properties.occlusionStrength = glm::clamp(occlusionStrength, 0.0f, 1.0f);
	this->properties.roughnessStrength = glm::clamp(roughnessStrength, 0.0f, 1.0f);
	this->properties.metallicStrength = glm::clamp(metallicStrength, 0.0f, 1.0f);

	/// Update the uniform properties to tell the shader to use the ORM map
	this->properties.useOcclusionMap = this->hasOrmMap ? 1.0f : 0.0f;
	this->properties.useRoughnessMap = this->hasOrmMap ? 1.0f : 0.0f;
	this->properties.useMetallicMap = this->hasOrmMap ? 1.0f : 0.0f;

	/// Update the descriptor set with the new texture
	this->updateTextureDescriptors();

	/// Update the uniform buffer to reflect the changes
	this->updateUniformBuffer();

	spdlog::debug("Set ORM map for material '{}': {} (O:{}/R:{}/M:{})",
		this->name,
		this->hasOrmMap ? texture->getName() : "none",
		this->properties.occlusionStrength,
		this->properties.roughnessStrength,
		this->properties.metallicStrength);
}

void PBRMaterial::setTextureTiling(float uTiling, float vTiling) {
	/// Set the same tiling for all texture types
	/// This provides a convenient way to adjust all texture coordinates uniformly
	this->properties.albedoTiling = glm::vec2(uTiling, vTiling);
	this->properties.normalTiling = glm::vec2(uTiling, vTiling);
	this->properties.roughnessTiling = glm::vec2(uTiling, vTiling);
	this->properties.metallicTiling = glm::vec2(uTiling, vTiling);
	this->properties.occlusionTiling = glm::vec2(uTiling, vTiling);

	/// Update the uniform buffer to reflect the changes
	this->updateUniformBuffer();

	spdlog::debug("Set global texture tiling to ({}, {}) for material '{}'",
		uTiling, vTiling, this->name);
}

void PBRMaterial::setTextureTiling(TextureType textureType, float uTiling, float vTiling) {
	/// Set tiling for a specific texture type
	/// This allows fine-grained control over individual texture coordinates
	switch (textureType) {
		case TextureType::Albedo:
			this->properties.albedoTiling = glm::vec2(uTiling, vTiling);
			break;
		case TextureType::Normal:
			this->properties.normalTiling = glm::vec2(uTiling, vTiling);
			break;
		case TextureType::Roughness:
			this->properties.roughnessTiling = glm::vec2(uTiling, vTiling);
			break;
		case TextureType::Metallic:
			this->properties.metallicTiling = glm::vec2(uTiling, vTiling);
			break;
		case TextureType::Occlusion:
			this->properties.occlusionTiling = glm::vec2(uTiling, vTiling);
			break;
		case TextureType::RoughnessMetallic:
			this->properties.roughnessTiling = glm::vec2(uTiling, vTiling);
			this->properties.metallicTiling = glm::vec2(uTiling, vTiling);
			break;
		case TextureType::OcclusionRoughnessMetallic:
			this->properties.occlusionTiling = glm::vec2(uTiling, vTiling);
			this->properties.roughnessTiling = glm::vec2(uTiling, vTiling);
			this->properties.metallicTiling = glm::vec2(uTiling, vTiling);
			break;
	}

	/// Update the uniform buffer to reflect the changes
	this->updateUniformBuffer();

	spdlog::debug("Set texture tiling for type {} to ({}, {}) for material '{}'",
		static_cast<int>(textureType), uTiling, vTiling, this->name);
}

void PBRMaterial::bind(VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout) const {
	spdlog::trace("Binding material '{}' with descriptors: albedo={}, normal={}, roughness={}, metallic={}, occlusion={}",
		this->name,
		this->albedoTexture ? "yes" : "no",
		this->normalMap ? "yes" : "no",
		this->roughnessMap ? "yes" : "no",
		this->metallicMap ? "yes" : "no",
		this->occlusionMap ? "yes" : "no");

	/// Call the base class implementation first
	/// This ensures we maintain any binding behavior from the base Material class
	Material::bind(cmdBuffer, pipelineLayout);

	/// Bind all textures to their respective binding points
	/// Since we're using set 2 for material properties, we need to bind our descriptor set to set 2
	vkCmdBindDescriptorSets(
		cmdBuffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		pipelineLayout,
		2,  /// Set index 2 for material properties (after camera and lights)
		1,  /// Number of descriptor sets to bind
		&this->descriptorSet, /// The descriptor set containing our textures
		0, nullptr /// No dynamic offsets
	);
}

VkDescriptorSetLayout PBRMaterial::getDescriptorSetLayout() const {
	/// Return the descriptor set layout
	/// This is needed during pipeline creation to tell Vulkan what resources the shader expects
	return this->descriptorSetLayout.get();
}

void PBRMaterial::createDescriptorSetLayout() {
	/// Create descriptor set layout for PBR materials
	/// The layout defines what resources (uniform buffers, textures) our shaders can access
	/// This layout must match the binding points defined in our shaders

	/// We define 6 bindings:
	/// - Binding 0: Material uniform buffer (properties)
	/// - Binding 1: Albedo texture
	/// - Binding 2: Normal map
	/// - Binding 3: Roughness map or combined map
	/// - Binding 4: Metallic map or combined map
	/// - Binding 5: Occlusion map or combined map
	///
	/// For combined textures, we reuse the same texture at multiple binding points
	/// and use channel masks in the shader to extract the correct values
	std::array<VkDescriptorSetLayoutBinding, 6> bindings{};

	/// Material properties uniform buffer (binding 0)
	/// This is used for base colors, factors, and texture flags
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[0].descriptorCount = 1;
	/// Used in fragment shader for PBR calculations
	bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[0].pImmutableSamplers = nullptr;

	/// Albedo texture (binding 1)
	/// This provides the base color of the material
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[1].pImmutableSamplers = nullptr;

	/// Normal map texture (binding 2)
	/// This provides surface detail through normal perturbation
	bindings[2].binding = 2;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[2].descriptorCount = 1;
	/// We might use normal mapping in both vertex and fragment shaders in the future,
	/// but for now we'll keep it in the fragment shader only
	bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[2].pImmutableSamplers = nullptr;

	/// Roughness map texture (binding 3)
	/// This controls microfacet distribution (surface roughness)
	bindings[3].binding = 3;
	bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[3].descriptorCount = 1;
	bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[3].pImmutableSamplers = nullptr;

	/// Metallic map texture (binding 4)
	/// This controls which parts of the surface are metallic vs. dielectric
	bindings[4].binding = 4;
	bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[4].descriptorCount = 1;
	bindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[4].pImmutableSamplers = nullptr;

	/// Occlusion map texture (binding 5)
	/// This approximates ambient occlusion for more realistic lighting
	bindings[5].binding = 5;
	bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[5].descriptorCount = 1;
	bindings[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[5].pImmutableSamplers = nullptr;

	/// Create the descriptor set layout with all our bindings
	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	/// Optional: define binding flags if needed (e.g., for variable descriptor counts or partially bound resources)
	/// We could use VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT to allow unbound texture samplers
	/// This would let us create materials without all textures, but requires Vulkan 1.2+
	/// For now, we'll leave this as nullptr and handle unbound textures in the shader
	layoutInfo.pNext = nullptr;

	VkDescriptorSetLayout layout;
	VK_CHECK(vkCreateDescriptorSetLayout(
		this->device,
		&layoutInfo,
		nullptr,
		&layout
	));

	/// Store the layout in our RAII wrapper for automatic cleanup
	this->descriptorSetLayout = vulkan::VulkanDescriptorSetLayoutHandle(
		layout,
		[this](VkDescriptorSetLayout l) {
			vkDestroyDescriptorSetLayout(this->device, l, nullptr);
		}
	);

	spdlog::debug("Created descriptor set layout for PBR material '{}' with {} bindings",
		this->name, bindings.size());
}

void PBRMaterial::createUniformBuffer() {
	/// Calculate the buffer size required for our properties structure
	/// We need to consider alignment requirements for the GPU
	/// Using sizeof directly works for host-visible memory but may not be optimal
	VkDeviceSize bufferSize = sizeof(Properties);

	/// Add padding to ensure the buffer size meets alignment requirements
	/// Most GPUs require at least 256-byte alignment for optimal performance
	/// However, for small uniform buffers this may be overkill
	const VkDeviceSize minAlignment = 64; // Common alignment size for uniform buffers
	bufferSize = (bufferSize + minAlignment - 1) & ~(minAlignment - 1);

	/// Create the uniform buffer
	/// We use VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT to indicate this is a uniform buffer
	/// Host visible memory allows CPU updates, coherent memory ensures updates are visible
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = bufferSize;
	bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; /// Only used by one queue family

	VkBuffer uniformBuffer;
	VK_CHECK(vkCreateBuffer(
		this->device,
		&bufferInfo,
		nullptr,
		&uniformBuffer
	));

	/// Store the buffer in an RAII handle for automatic cleanup
	this->uniformBuffer = vulkan::VulkanBufferHandle(
		uniformBuffer,
		[this](VkBuffer b) {
			vkDestroyBuffer(this->device, b, nullptr);
		}
	);

	/// Query memory requirements for proper allocation
	/// This ensures we select the right memory type and size
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(this->device, this->uniformBuffer.get(), &memRequirements);

	/// Allocate memory for the uniform buffer
	/// We need memory that is host-visible (CPU can write to it) and
	/// host-coherent (CPU writes are automatically visible to GPU)
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

	/// Bind the memory to the buffer
	/// This connects the allocated memory to the buffer object
	VK_CHECK(vkBindBufferMemory(
		this->device,
		this->uniformBuffer.get(),
		this->uniformBufferMemory,
		0
	));

	/// Initialize the uniform buffer with default values
	/// This ensures the shader has valid data even before any properties are set
	this->updateUniformBuffer();

	spdlog::debug("Created uniform buffer for PBR material '{}' with size {} bytes (aligned from {})",
		this->name, bufferSize, sizeof(Properties));
}

void PBRMaterial::createDescriptorSet() {
	/// Validate that the descriptor pool exists
	/// This prevents crashes when trying to allocate from a null pool
	if (!this->descriptorPool || !this->descriptorPool.get()) {
		throw vulkan::VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Cannot create descriptor set: descriptor pool is null",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	/// Allocate a descriptor set from our pool
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = this->descriptorPool.get();
	allocInfo.descriptorSetCount = 1;
	const VkDescriptorSetLayout layout = this->descriptorSetLayout.get();
	allocInfo.pSetLayouts = &layout;

	/// Allocate the descriptor set
	VK_CHECK(vkAllocateDescriptorSets(
		this->device,
		&allocInfo,
		&this->descriptorSet
	));

	/// Update the descriptor set for the uniform buffer (binding 0)
	/// We need to do this immediately because the uniform buffer is always present
	VkDescriptorBufferInfo bufferInfo{};
	bufferInfo.buffer = this->uniformBuffer.get();
	bufferInfo.offset = 0;
	bufferInfo.range = sizeof(Properties);

	VkWriteDescriptorSet descriptorWrite{};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = this->descriptorSet;
	descriptorWrite.dstBinding = 0; /// Uniform buffer is always at binding 0
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pBufferInfo = &bufferInfo;

	/// Update the descriptor set with the uniform buffer binding
	vkUpdateDescriptorSets(
		this->device,
		1,
		&descriptorWrite,
		0,
		nullptr
	);

	/// For textures, we'll update the descriptor bindings on-demand as textures are assigned
	/// This is more efficient than creating default textures for all possible bindings up front
	/// The updateTextureDescriptors() method handles this when textures are set
	this->updateTextureDescriptors();

	spdlog::debug("Created and initialized descriptor set for PBR material '{}'", this->name);
}

void PBRMaterial::updateUniformBuffer() {
	/// Begin a critical section for thread safety
	/// This prevents multiple threads from updating the uniform buffer simultaneously
	/// In a more complex application, we would use a proper mutex here

	/// Map the uniform buffer memory to get a CPU-accessible pointer
	void* data;
	VK_CHECK(vkMapMemory(
		this->device,
		this->uniformBufferMemory,
		0,
		sizeof(Properties), /// We only map what we need, not the entire aligned size
		0, /// No special flags
		&data
	));

	/// Copy our properties structure to the mapped memory
	/// This is a direct memory copy, which is efficient but requires careful alignment
	/// The properties struct should be designed for GPU memory layout
	std::memcpy(data, &this->properties, sizeof(Properties));

	/// Add debug validation in development builds
/// This helps catch data corruption issues early
#ifdef _DEBUG
	/// Verify the copied data matches our source data
	Properties* mappedProps = static_cast<Properties*>(data);
	assert(std::memcmp(mappedProps, &this->properties, sizeof(Properties)) == 0);
#endif

	/// Unmap the memory
	/// Since we're using VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, we don't need
	/// to explicitly flush the memory - the driver handles synchronization
	vkUnmapMemory(this->device, this->uniformBufferMemory);

	/// Log the update at trace level for debugging
	/// This is useful for tracking material property changes
	spdlog::trace("Updated uniform buffer for PBR material '{}'", this->name);
}

void PBRMaterial::updateTextureDescriptors() {
	/// Update the descriptor set with all our current textures
	/// This connects our texture images to the shader binding points
	///
	/// The texture binding scheme is designed to be flexible:
	/// - Each texture type has a dedicated binding point
	/// - Combined textures are bound at multiple points when needed
	/// - The shader uses uniform flags to know which textures to sample
	/// - Binding points are kept consistent to simplify shader logic

	/// Vector to hold our descriptor writes and image infos
	/// We need to keep these alive until vkUpdateDescriptorSets is called
	std::vector<VkDescriptorImageInfo> imageInfos;
	std::vector<VkWriteDescriptorSet> descriptorWrites;

	/// Pre-allocate space for our vectors
	/// Maximum case: 5 textures (albedo, normal, roughness, metallic, occlusion)
	imageInfos.reserve(5);
	descriptorWrites.reserve(5);

	/// Handle albedo texture (binding 1)
	if (this->hasAlbedoTexture && this->albedoTexture) {
		imageInfos.emplace_back(VkDescriptorImageInfo{
			this->albedoTexture->getSampler(),
			this->albedoTexture->getImageView(),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		});

		descriptorWrites.emplace_back(VkWriteDescriptorSet{});
		auto& write = descriptorWrites.back();
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = this->descriptorSet;
		write.dstBinding = 1; /// Albedo texture binding
		write.dstArrayElement = 0;
		write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write.descriptorCount = 1;
		write.pImageInfo = &imageInfos.back();
	}

	/// Handle normal map (binding 2)
	if (this->hasNormalMap && this->normalMap) {
		imageInfos.emplace_back(VkDescriptorImageInfo{
			this->normalMap->getSampler(),
			this->normalMap->getImageView(),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		});

		descriptorWrites.emplace_back(VkWriteDescriptorSet{});
		auto& write = descriptorWrites.back();
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = this->descriptorSet;
		write.dstBinding = 2; /// Normal map binding
		write.dstArrayElement = 0;
		write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write.descriptorCount = 1;
		write.pImageInfo = &imageInfos.back();
	}

	/// Handle roughness map (binding 3)
	/// We use a priority system:
	/// 1. Individual roughness map if available
	/// 2. Combined roughness-metallic map if available
	/// 3. Combined ORM map if available
	std::shared_ptr<Texture> roughnessTexture = nullptr;

	if (this->hasRoughnessMap && this->roughnessMap) {
		roughnessTexture = this->roughnessMap;
	} else if (this->hasRoughnessMetallicMap && this->roughnessMetallicMap) {
		roughnessTexture = this->roughnessMetallicMap;
	} else if (this->hasOrmMap && this->ormMap) {
		roughnessTexture = this->ormMap;
	}

	if (roughnessTexture) {
		imageInfos.emplace_back(VkDescriptorImageInfo{
			roughnessTexture->getSampler(),
			roughnessTexture->getImageView(),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		});

		descriptorWrites.emplace_back(VkWriteDescriptorSet{});
		auto& write = descriptorWrites.back();
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = this->descriptorSet;
		write.dstBinding = 3; /// Roughness map binding
		write.dstArrayElement = 0;
		write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write.descriptorCount = 1;
		write.pImageInfo = &imageInfos.back();
	}

	/// Handle metallic map (binding 4)
	/// Similar priority system as roughness
	std::shared_ptr<Texture> metallicTexture = nullptr;

	if (this->hasMetallicMap && this->metallicMap) {
		metallicTexture = this->metallicMap;
	} else if (this->hasRoughnessMetallicMap && this->roughnessMetallicMap) {
		metallicTexture = this->roughnessMetallicMap;
	} else if (this->hasOrmMap && this->ormMap) {
		metallicTexture = this->ormMap;
	}

	if (metallicTexture) {
		imageInfos.emplace_back(VkDescriptorImageInfo{
			metallicTexture->getSampler(),
			metallicTexture->getImageView(),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		});

		descriptorWrites.emplace_back(VkWriteDescriptorSet{});
		auto& write = descriptorWrites.back();
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = this->descriptorSet;
		write.dstBinding = 4; /// Metallic map binding
		write.dstArrayElement = 0;
		write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write.descriptorCount = 1;
		write.pImageInfo = &imageInfos.back();
	}

	/// Handle occlusion map (binding 5)
	std::shared_ptr<Texture> occlusionTexture = nullptr;

	if (this->hasOcclusionMap && this->occlusionMap) {
		occlusionTexture = this->occlusionMap;
	} else if (this->hasOrmMap && this->ormMap) {
		occlusionTexture = this->ormMap;
	}

	if (occlusionTexture) {
		imageInfos.emplace_back(VkDescriptorImageInfo{
			occlusionTexture->getSampler(),
			occlusionTexture->getImageView(),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		});

		descriptorWrites.emplace_back(VkWriteDescriptorSet{});
		auto& write = descriptorWrites.back();
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = this->descriptorSet;
		write.dstBinding = 5; /// Occlusion map binding
		write.dstArrayElement = 0;
		write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write.descriptorCount = 1;
		write.pImageInfo = &imageInfos.back();
	}

	/// Perform the actual descriptor update
	/// Only update if we have descriptors to write
	/// This prevents unnecessary Vulkan API calls
	if (!descriptorWrites.empty()) {
		/// Update all descriptor sets at once
		/// This is more efficient than individual updates
		vkUpdateDescriptorSets(
			this->device,
			static_cast<uint32_t>(descriptorWrites.size()),
			descriptorWrites.data(),
			0,
			nullptr
		);

		spdlog::debug("Updated texture descriptors for PBR material '{}' with {} textures",
			this->name, descriptorWrites.size());
	}
}

uint32_t PBRMaterial::channelToMask(TextureChannel channel) const {
	/// Convert a texture channel enum to a bit mask for the shader
	/// We use a simple uint32_t as the channel index (0=R, 1=G, 2=B, 3=A)
	/// This is simpler to pass to the shader than an enum
	switch (channel) {
		case TextureChannel::R: return 0;
		case TextureChannel::G: return 1;
		case TextureChannel::B: return 2;
		case TextureChannel::A: return 3;
		default: return 0; /// Default to R channel if invalid
	}
}

bool PBRMaterial::createDescriptorPool() {
	/// Calculate the total number of descriptors needed for this material
	/// We need:
	/// - 1 uniform buffer descriptor for material properties
	/// - Up to 5 combined image sampler descriptors for textures (albedo, normal, roughness, metallic, occlusion)
	///
	/// We allocate the maximum number even if not all textures will be used
	/// This simplifies descriptor management and allows for adding textures later without recreating the pool
	constexpr uint32_t uniformBufferCount = 1;
	constexpr uint32_t samplerCount = 5;

	/// Define pool sizes for our different descriptor types
	std::array<VkDescriptorPoolSize, 2> poolSizes{};

	/// Uniform buffer pool size
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = uniformBufferCount;

	/// Combined image sampler pool size for textures
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = samplerCount;

	/// Create the descriptor pool with enough space for all possible descriptors
	/// We use VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT to allow freeing individual sets
	/// This is useful if we need to recreate descriptor sets when textures change
	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = 1; /// We only need one descriptor set per material

	VkDescriptorPool descriptorPool;
	try {
		VK_CHECK(vkCreateDescriptorPool(this->device, &poolInfo, nullptr, &descriptorPool));

		/// Wrap the pool in an RAII handle for automatic cleanup
		this->descriptorPool
			= vulkan::VulkanDescriptorPoolHandle(descriptorPool, [this](VkDescriptorPool p) {
				  vkDestroyDescriptorPool(this->device, p, nullptr);
			  });

		spdlog::debug(
			"Created descriptor pool for PBR material '{}' with {} UBO and {} sampler descriptors",
			this->name,
			uniformBufferCount,
			samplerCount);

		return true;
	} catch (const vulkan::VulkanException &e) {
		spdlog::error(
			"Failed to create descriptor pool for PBR material '{}': {}", this->name, e.what());
		return false;
	}
}

#ifdef _DEBUG
/// Debug method to validate uniform buffer contents
/// This helps detect memory corruption or improper updates
/// Only available in debug builds to avoid performance impact
void PBRMaterial::validateUniformBuffer() const {
	/// Map the uniform buffer to inspect its contents
	void* data;
	VK_CHECK(vkMapMemory(
		this->device,
		this->uniformBufferMemory,
		0,
		sizeof(Properties),
		0,
		&data
	));

	/// Cast to our Properties struct for easier inspection
	Properties* mappedProps = static_cast<Properties*>(data);

	/// Verify each property matches what we expect
	bool valid = true;
	if (std::memcmp(&mappedProps->baseColor, &this->properties.baseColor, sizeof(glm::vec4)) != 0) {
		spdlog::error("Uniform buffer validation failed: baseColor mismatch");
		valid = false;
	}

	if (mappedProps->roughness != this->properties.roughness ||
		mappedProps->metallic != this->properties.metallic ||
		mappedProps->ambient != this->properties.ambient) {
		spdlog::error("Uniform buffer validation failed: basic properties mismatch");
		valid = false;
		}

	if (mappedProps->useAlbedoTexture != this->properties.useAlbedoTexture ||
		mappedProps->useNormalMap != this->properties.useNormalMap ||
		mappedProps->useRoughnessMap != this->properties.useRoughnessMap ||
		mappedProps->useMetallicMap != this->properties.useMetallicMap ||
		mappedProps->useOcclusionMap != this->properties.useOcclusionMap) {
		spdlog::error("Uniform buffer validation failed: texture flags mismatch");
		valid = false;
		}

	/// Print validation result
	if (valid) {
		spdlog::debug("Uniform buffer validation passed for material '{}'", this->name);
	} else {
		spdlog::error("Uniform buffer validation failed for material '{}'", this->name);
	}

	/// Unmap the memory
	vkUnmapMemory(this->device, this->uniformBufferMemory);
}
#endif

} /// namespace lillugsi::rendering