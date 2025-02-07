#include "terrainmaterial.h"
#include "vulkan/vulkanexception.h"
#include "vulkan/vulkanutils.h"
#include <spdlog/spdlog.h>

namespace lillugsi::rendering {

TerrainMaterial::TerrainMaterial(
	VkDevice device,
	const std::string& name,
	VkPhysicalDevice physicalDevice)
	: Material(device, name, physicalDevice, MaterialType::Custom)
	, vertexShaderPath(DefaultVertexShaderPath)
	, fragmentShaderPath(DefaultFragmentShaderPath) {

	/// Initialize default biome parameters
	/// We start with a simple height-based gradient from ocean to mountains
	/// These defaults provide a basic visualization for testing
	/// Initialize default biome parameters
	/// We set up a basic Earth-like gradient with steepness constraints
	this->properties.biomes[0] = {
		glm::vec4(0.0f, 0.1f, 0.4f, 1.0f),  /// Deep ocean blue
		0.0f,                                /// Start at lowest point
		0.45f,                               /// Up to 45% height
		1.0f,                                /// Water appears only on any steepness until we have removed the elevation for water
		0.1f                                 /// Smooth water surface
	};

	this->properties.biomes[1] = {
		glm::vec4(0.8f, 0.7f, 0.5f, 1.0f),  /// Sandy beaches
		0.38f,                              /// Slight overlap with water
		0.5f,                               /// Up to midlands
		0.4f,                               /// Beaches form on moderate slopes
		0.7f                                /// Rough sandy texture
	};

	this->properties.biomes[2] = {
		glm::vec4(0.2f, 0.5f, 0.2f, 1.0f),  /// Green midlands
		0.48f,                              /// Overlap with beaches
		0.7f,                               /// Up to mountain zone
		0.6f,                               /// Grass grows on most slopes
		0.5f                                /// Medium roughness
	};

	this->properties.biomes[3] = {
		glm::vec4(0.95f, 0.95f, 0.95f, 1.0f), /// Snow peaks
		0.68f,                                /// Overlap with midlands
		1.0f,                                 /// Up to highest point
		0.4f,                                 /// Snow only accumulates on gentler slopes
		0.66f                                 /// Fairly smooth snow surface
	};

	/// Set default planet radius
	/// This can be adjusted later based on actual planet size
	this->properties.planetRadius = 1.0f;

	/// Create descriptor layout first as it's needed for other resources
	this->createDescriptorSetLayout();

	/// Create and initialize the uniform buffer
	this->createUniformBuffer();

	/// Create descriptor pool and set
	this->createDescriptorPool();
	this->createDescriptorSet();

	spdlog::debug("Created terrain material '{}' with default biome parameters",
		this->name);
}

TerrainMaterial::~TerrainMaterial() {
	/// Clean up uniform buffer memory
	/// Base class and RAII handles handle other cleanup
	if (this->uniformBufferMemory != VK_NULL_HANDLE) {
		vkFreeMemory(this->device, this->uniformBufferMemory, nullptr);
	}

	spdlog::debug("Destroyed terrain material '{}'", this->name);
}

ShaderPaths TerrainMaterial::getShaderPaths() const {
	/// Return stored shader paths for pipeline creation
	ShaderPaths paths;
	paths.vertexPath = this->vertexShaderPath;
	paths.fragmentPath = this->fragmentShaderPath;

	/// Validate paths before returning
	/// This helps catch configuration errors early
	if (!paths.isValid()) {
		throw vulkan::VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Invalid shader paths in terrain material '" + this->name + "'",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	return paths;
}

void TerrainMaterial::setBiome(uint32_t index, const glm::vec4& color,
	float minHeight, float maxHeight) {
	/// Validate index range
	if (index >= 4) {
		throw vulkan::VulkanException(
			VK_ERROR_VALIDATION_FAILED_EXT,
			"Invalid biome index in terrain material '" + this->name + "'",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	/// Validate height range
	/// Heights should be normalized between 0 and 1
	if (minHeight < 0.0f || minHeight > 1.0f ||
		maxHeight < 0.0f || maxHeight > 1.0f ||
		minHeight >= maxHeight) {
		throw vulkan::VulkanException(
			VK_ERROR_VALIDATION_FAILED_EXT,
			"Invalid height range for biome in terrain material '" + this->name + "'",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	/// Update biome parameters
	this->properties.biomes[index].color = color;
	this->properties.biomes[index].minHeight = minHeight;
	this->properties.biomes[index].maxHeight = maxHeight;

	/// Update GPU data
	this->updateUniformBuffer();

	spdlog::debug("Updated biome {} in material '{}': color({}, {}, {}), height range [{}, {}]",
		index, this->name, color.r, color.g, color.b, minHeight, maxHeight);
}

void TerrainMaterial::setPlanetRadius(float radius) {
	/// Validate radius
	if (radius <= 0.0f) {
		throw vulkan::VulkanException(
			VK_ERROR_VALIDATION_FAILED_EXT,
			"Invalid radius in terrain material '" + this->name + "'",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	this->properties.planetRadius = radius;
	this->updateUniformBuffer();

	spdlog::debug("Updated planet radius to {} in material '{}'",
		radius, this->name);
}

void TerrainMaterial::createDescriptorSetLayout() {
	/// Create the descriptor layout for our uniform buffer
	VkDescriptorSetLayoutBinding binding{};
	binding.binding = 0;
	binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	binding.descriptorCount = 1;
	binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;  /// Used in fragment shader for biome coloring
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

	spdlog::debug("Created descriptor set layout for terrain material '{}'", this->name);
}

void TerrainMaterial::createUniformBuffer() {
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

	VK_CHECK(vkAllocateMemory(this->device, &allocInfo, nullptr, &this->uniformBufferMemory));
	VK_CHECK(vkBindBufferMemory(this->device, this->uniformBuffer.get(), this->uniformBufferMemory, 0));

	/// Initialize buffer with default properties
	this->updateUniformBuffer();

	spdlog::debug("Created uniform buffer for terrain material '{}'", this->name);
}

void TerrainMaterial::createDescriptorSet() {
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

	spdlog::debug("Created descriptor set for terrain material '{}'", this->name);
}

void TerrainMaterial::updateUniformBuffer() {
	/// Map memory and update uniform buffer contents
	void* data;
	VK_CHECK(vkMapMemory(this->device, this->uniformBufferMemory, 0, sizeof(Properties), 0, &data));
	memcpy(data, &this->properties, sizeof(Properties));
	vkUnmapMemory(this->device, this->uniformBufferMemory);

	spdlog::trace("Updated uniform buffer for terrain material '{}'", this->name);
}

} /// namespace lillugsi::rendering