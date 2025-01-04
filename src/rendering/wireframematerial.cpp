#include "wireframematerial.h"
#include "vulkan/vulkanexception.h"
#include "vulkan/vulkanutils.h"
#include <spdlog/spdlog.h>

namespace lillugsi::rendering {

WireframeMaterial::WireframeMaterial(
	VkDevice device,
	const std::string& name,
	VkPhysicalDevice physicalDevice)
	: Material(device, name, physicalDevice, MaterialType::Wireframe) {

	/// Create descriptor layout first as it's needed for other resources
	/// This establishes the interface between our material and shaders
	this->createDescriptorSetLayout();

	/// Create and initialize the uniform buffer for material properties
	/// This provides GPU access to our color settings
	this->createUniformBuffer();

	/// Create descriptor pool and set
	/// These connect our uniform buffer to the shader pipeline
	this->createDescriptorPool();
	this->createDescriptorSet();

	spdlog::debug("Created wireframe material '{}' with default white color", this->name);
}

WireframeMaterial::~WireframeMaterial() {
	/// Clean up uniform buffer memory
	/// The base Material class handles other cleanup
	if (this->uniformBufferMemory != VK_NULL_HANDLE) {
		vkFreeMemory(this->device, this->uniformBufferMemory, nullptr);
	}

	spdlog::debug("Destroyed wireframe material '{}'", this->name);
}

ShaderPaths WireframeMaterial::getShaderPaths() const {
	/// Return shader paths for wireframe rendering
	/// We use specialized shaders optimized for line rendering
	ShaderPaths paths;
	paths.vertexPath = vertexShaderPath;
	paths.fragmentPath = fragmentShaderPath;

	/// Validate shader paths before returning
	if (!paths.isValid()) {
		throw vulkan::VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Invalid shader paths in wireframe material '" + this->name + "'",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	return paths;
}

void WireframeMaterial::setColor(const glm::vec3& color) {
	/// Update material color and sync with GPU
	this->properties.color = color;
	this->updateUniformBuffer();

	spdlog::trace("Set wireframe color to ({}, {}, {}) for material '{}'",
		color.r, color.g, color.b, this->name);
}

void WireframeMaterial::configurePipeline(vulkan::PipelineConfig& config) const {
	/// Configure pipeline for wireframe rendering
	/// We override the base configuration to set wireframe-specific states

	/// Set polygon mode to line for wireframe display
	/// This is the key configuration that enables wireframe rendering
	config.setRasterization(
		VK_POLYGON_MODE_LINE,            /// Draw lines instead of filled polygons
		VK_CULL_MODE_NONE,               /// Disable culling to see all lines
		VK_FRONT_FACE_COUNTER_CLOCKWISE  /// Standard winding order
	);

	/// Configure depth testing
	/// We want lines to be visible even when they're "behind" other lines
	config.setDepthState(
		true,                       /// Enable depth testing
		true,                       /// Enable depth writes
		VK_COMPARE_OP_LESS_OR_EQUAL /// Standard depth comparison
	);

	/// Configure blending for lines
	/// We use alpha blending to support translucent lines if needed
	config.setBlendState(
		true,                                    /// Enable blending
		VK_BLEND_FACTOR_SRC_ALPHA,              /// Source color factor
		VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,    /// Destination color factor
		VK_BLEND_OP_ADD,                        /// Color blend operation
		VK_BLEND_FACTOR_ONE,                    /// Source alpha factor
		VK_BLEND_FACTOR_ZERO,                   /// Destination alpha factor
		VK_BLEND_OP_ADD                         /// Alpha blend operation
	);
}

void WireframeMaterial::createDescriptorSetLayout() {
	/// Create the descriptor layout for our uniform buffer
	/// We only need one binding for the color data
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

	/// Wrap the layout in our RAII handle
	this->descriptorSetLayout = vulkan::VulkanDescriptorSetLayoutHandle(
		layout,
		[this](VkDescriptorSetLayout l) {
			vkDestroyDescriptorSetLayout(this->device, l, nullptr);
		});

	spdlog::debug("Created descriptor set layout for wireframe material '{}'", this->name);
}

void WireframeMaterial::createUniformBuffer() {
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

	spdlog::debug("Created uniform buffer for wireframe material '{}'", this->name);
}

void WireframeMaterial::createDescriptorSet() {
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

	spdlog::debug("Created descriptor set for wireframe material '{}'", this->name);
}

void WireframeMaterial::updateUniformBuffer() {
	/// Map memory and update uniform buffer contents
	void* data;
	VK_CHECK(vkMapMemory(this->device, this->uniformBufferMemory, 0, sizeof(Properties), 0, &data));
	memcpy(data, &this->properties, sizeof(Properties));
	vkUnmapMemory(this->device, this->uniformBufferMemory);

	spdlog::trace("Updated uniform buffer for wireframe material '{}'", this->name);
}

} /// namespace lillugsi::rendering