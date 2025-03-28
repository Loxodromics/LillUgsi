#include "terrainmaterial.h"
#include "vulkan/vulkanexception.h"
#include "vulkan/vulkanutils.h"
#include <spdlog/spdlog.h>

namespace lillugsi::rendering {

TerrainMaterial::TerrainMaterial(
	VkDevice device, const std::string &name, VkPhysicalDevice physicalDevice)
	: Material(device, name, physicalDevice, MaterialType::Custom)
	, vertexShaderPath(DefaultVertexShaderPath)
	, fragmentShaderPath(DefaultFragmentShaderPath) {
	/// Initialize default biome parameters
	/// Each biome has distinct physical properties that work together with its colors
	/// to create a convincing material appearance

	/// Deep oceans
	/// Water is handled as a special case - highly reflective with low roughness
	/// Underwater cliffs are rough and less metallic to suggest rock formations
	uint32_t biomeId = 0;
	/// Deep oceans
	/// Characterized by smooth, large-scale transitions and gentle wave patterns
	this->properties.biomes[biomeId] = {
		glm::vec4(0.0f, 0.1f, 0.4f, 1.0f),    /// Deep ocean blue
		glm::vec4(0.0f, 0.2f, 0.5f, 1.0f),    /// Slightly lighter blue for underwater cliffs
		0.0f,                                  /// Start at lowest point
		0.4f,                                  /// Up to 40% height
		0.3f,                                  /// Water appears only on flat areas
		0.2f,                                  /// Start showing underwater formations early
		0.1f,                                  /// Smooth water surface
		0.6f,                                  /// Rough underwater cliff surface
		0.9f,                                  /// Highly reflective water
		0.1f,                                  /// Less reflective underwater cliffs
		/// Water noise focuses on gentle, large-scale movement
		{
			1.0f,  /// Lower frequency for broad waves
			0.3f,  /// Subtle amplitude for gentle variation
			3,     /// Fewer octaves for smoother appearance
			0.5f,  /// Standard persistence
			2.0f   /// Standard lacunarity
		},
		biomeId,  /// Unique identifier
		0.0f,     /// Padding
		0.0f,     /// Padding
		0.0f,     /// Padding
		/// Transition to beach uses gentle, flowing patterns
		{
			0,    /// Simplex noise for smooth water transitions
			2.0f, /// Large scale for broad shoreline features
			0.3f, /// Soft edges for natural water boundaries
			0.0f, /// Padding
			{
				0.8f,  /// Medium frequency for water movement
				0.4f,  /// Moderate amplitude for subtle variation
				3,     /// Few octaves for smooth transitions
				0.5f,  /// Standard persistence
				2.0f   /// Standard lacunarity
			}
		}
	};

	/// Coastal regions and beaches
	/// Sand is rough and non-metallic, creating a diffuse appearance
	/// Sandstone cliffs are even rougher but maintain the same non-metallic quality
	/// Sand transitions use more detailed noise for realistic beach patterns
	biomeId++;
	this->properties.biomes[biomeId] = {
		glm::vec4(0.8f, 0.7f, 0.5f, 1.0f),    /// Sandy beach color
		glm::vec4(0.7f, 0.4f, 0.3f, 1.0f),    /// Reddish sandstone cliffs
		0.35f,                                 /// Overlap with water for shorelines
		0.5f,                                  /// Up to midlands
		0.6f,                                  /// Beaches form on moderate slopes
		0.4f,                                  /// Transition to cliffs at 40% steepness
		0.7f,                                  /// Rough sandy texture
		0.8f,                                  /// Very rough cliff texture
		0.0f,                                  /// Non-metallic sand
		0.0f,                                  /// Non-metallic cliffs
		/// Beach noise creates small dunes and ripples
		{
			4.0f,  /// Higher frequency for sand detail
			0.7f,  /// Strong amplitude for visible sand patterns
			4,     /// More octaves for detailed sand texture
			0.6f,  /// Slower falloff for richer detail
			2.5f   /// Wider frequency spread for varied patterns
		},
		biomeId,  /// Unique identifier
		0.0f,     /// Padding
		0.0f,     /// Padding
		0.0f,     /// Padding
		/// Transition to grass uses detailed patterns for natural beach borders
		{
			0,    /// Simplex noise for natural beach transitions
			3.0f, /// Medium scale for beach features
			0.7f, /// Moderately sharp edges for distinct beach boundaries
			0.0f, /// Padding
		{
			3.0f,  /// Higher frequency for detailed transitions
			0.6f,  /// Moderate amplitude for natural variation
			4,     /// More octaves for detailed boundaries
			0.5f,  /// Standard persistence
			2.0f   /// Standard lacunarity
		}
	}
};

	/// Midlands and forests
	/// Organic materials are non-metallic with medium roughness
	/// Rock faces are rougher but maintain non-metallic properties
	/// Organic materials with varied, natural transitions to create realistic vegetation boundaries
	biomeId++;
	this->properties.biomes[biomeId] = {
		glm::vec4(0.2f, 0.5f, 0.2f, 1.0f),    /// Green vegetation
		glm::vec4(0.5f, 0.5f, 0.5f, 1.0f),    /// Grey stone cliffs
		0.48f,                                 /// Overlap with beaches
		0.7f,                                  /// Up to mountain zone
		0.7f,                                  /// Vegetation on most slopes
		0.5f,                                  /// Show cliffs on steeper angles
		0.5f,                                  /// Medium vegetation roughness
		0.75f,                                 /// Rough rock texture
		0.0f,                                  /// Non-metallic vegetation
		0.0f,                                  /// Non-metallic rock
		/// Forest noise creates organic, varied patterns
		{
			3.0f,  /// Medium frequency for natural variation
			0.8f,  /// Strong amplitude for clear vegetation patterns
			5,     /// Many octaves for organic detail
			0.5f,  /// Standard persistence
			2.0f   /// Standard lacunarity
		},
		biomeId,  /// Unique identifier
		0.0f,     /// Padding
		0.0f,     /// Padding
		0.0f,     /// Padding
		/// Transition to mountains uses organic patterns for natural treeline
		{
			0,    /// Simplex noise for organic transitions
			4.0f, /// Medium-small scale for detailed vegetation boundaries
			0.6f, /// Moderate sharpness for natural treeline
			0.0f, /// Padding
			{
				4.0f,  /// Higher frequency for detailed vegetation patterns
				0.7f,  /// Strong amplitude for clear transitions
				5,     /// Many octaves for natural variation
				0.5f,  /// Standard persistence
				2.2f   /// Slightly higher lacunarity for more varied patterns
			}
		}
	};

	/// Mountain peaks
	/// Snow is smooth but not metallic
	/// Exposed granite is very rough and slightly metallic due to mineral content
	/// Sharp transitions with varied noise patterns to create realistic alpine environments
	biomeId++;
	this->properties.biomes[biomeId] = {
		glm::vec4(0.95f, 0.95f, 0.95f, 1.0f), /// Bright snow
		glm::vec4(0.3f, 0.3f, 0.3f, 1.0f),    /// Dark granite cliffs
		0.6f,                                  /// Overlap with midlands
		1.0f,                                  /// Up to highest point
		0.5f,                                  /// Snow on gentler slopes
		0.3f,                                  /// Quick transition to rock
		0.3f,                                  /// Smooth snow surface
		0.9f,                                  /// Very rough granite texture
		0.0f,                                  /// Non-metallic snow
		0.1f,                                  /// Slightly metallic granite
		/// Mountain noise creates windswept patterns and rocky detail
		{
			5.0f,  /// High frequency for detailed snow/rock patterns
			0.9f,  /// Strong amplitude for dramatic mountain features
			6,     /// Many octaves for rich detail
			0.5f,  /// Standard persistence
			2.2f   /// Slightly higher lacunarity for sharper features
		},
		biomeId,  /// Unique identifier
		0.0f,     /// Padding
		0.0f,     /// Padding
		0.0f,     /// Padding
		/// Transition parameters unused for highest biome
		{
			0,    /// Type unused
			1.0f, /// Scale unused
			1.0f, /// Sharpness unused
			0.0f, /// Padding
			{
				1.0f,  /// Unused
				1.0f,  /// Unused
				1,     /// Unused
				0.5f,  /// Unused
				2.0f   /// Unused
		}
	}
};

	/// Set number of active biomes
	this->properties.numBiomes = 4;

	/// Set default planet radius
	/// This can be adjusted later based on actual planet size
	this->properties.planetRadius = 1.0f;

	/// Start with normal rendering mode
	this->properties.debugMode = static_cast<uint32_t>(TerrainDebugMode::None);

	spdlog::info("TerrainMaterial initial values - planetRadius: {}, debugMode: {}",
	this->properties.planetRadius,
	this->properties.debugMode);

	/// Create descriptor layout first as it's needed for other resources
	this->createDescriptorSetLayout();

	/// Create and initialize the uniform buffer
	this->createUniformBuffer();

	/// Create descriptor pool and set
	this->createDescriptorPool();
	this->createDescriptorSet();

	spdlog::debug("Created terrain material '{}' with default biome parameters", this->name);
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
			__FUNCTION__,
			__FILE__,
			__LINE__);
	}

	this->properties.planetRadius = radius;
	this->updateUniformBuffer();

	spdlog::debug("Updated planet radius to {} in material '{}'", radius, this->name);
}

void TerrainMaterial::setNoiseParameters(const uint32_t index, const NoiseParameters& params) {
	/// Validate index before proceeding
	if (index >= 4) {
		throw vulkan::VulkanException(
			VK_ERROR_VALIDATION_FAILED_EXT,
			"Invalid biome index in terrain material '" + this->name + "'",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	/// Update the parameters for the specified biome
	this->properties.biomes[index].noise = params;

	/// Sync changes to GPU
	this->updateUniformBuffer();

	spdlog::debug("Updated noise parameters for biome {} in material '{}'",
		index, this->name);
}

void TerrainMaterial::setDebugMode(const TerrainDebugMode mode) {
	/// Update debug mode and sync to GPU
	this->properties.debugMode = static_cast<uint32_t>(mode);
	this->updateUniformBuffer();

	spdlog::info("Set debug mode {} for terrain material '{}'",
		static_cast<uint32_t>(mode), this->name);
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
	/// Verify that Properties structure meets GPU alignment requirements
	static_assert(sizeof(Properties) % 16 == 0,
		"Properties structure size must be a multiple of 16 bytes");

	/// Create the uniform buffer for material properties
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = sizeof(Properties);  /// Size now includes new parameters
	bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	/// Log the buffer size for debugging
	spdlog::debug("Creating uniform buffer with size: {} bytes", bufferInfo.size);


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

	spdlog::debug("Created uniform buffer for terrain material '{}' with size {} bytes",
		this->name, bufferInfo.size);
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
	void* data;
	VK_CHECK(vkMapMemory(this->device, this->uniformBufferMemory, 0, sizeof(Properties), 0, &data));

	// Log some values before copying
	spdlog::debug("Updating uniform buffer - planetRadius: {}, debugMode: {}",
		this->properties.planetRadius,
		this->properties.debugMode);

	memcpy(data, &this->properties, sizeof(Properties));
	vkUnmapMemory(this->device, this->uniformBufferMemory);
}

} /// namespace lillugsi::rendering