#include "vulkan/vulkanexception.h"
#include "vulkan/vulkanformatters.h"
#include "material.h"
#include "vertex.h"
#include <spdlog/spdlog.h>

namespace lillugsi::rendering {

Material::Material(VkDevice device,
	const std::string& name,
	VkPhysicalDevice physicalDevice,
	MaterialType type,
	MaterialFeatureFlags features)
	: device(device)
	, physicalDevice(physicalDevice)
	, name(name)
	, materialType(type)
	, features(features) {

	spdlog::debug("Creating {} material '{}' with features {:#x}",
		getMaterialTypeName(type), name, static_cast<uint32_t>(features));
}

void Material::bind(VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout) const {
	/// Bind the material's descriptor set to set index 2
	/// We use set 0 for camera data and set 1 for lighting
	VkDescriptorSet sets[] = {this->descriptorSet};
	vkCmdBindDescriptorSets(cmdBuffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		pipelineLayout,
		2, 1, sets,
		0, nullptr);

	spdlog::trace("Bound descriptor sets for material '{}'", this->name);
}

bool Material::createDescriptorPool() {
	/// Define pool sizes for our different descriptor types
	/// We need both uniform buffers and combined image samplers
	std::array<VkDescriptorPoolSize, 2> poolSizes{};

	/// Uniform buffer pool size
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = 1;

	/// Combined image sampler pool size
	/// This is for texture samplers
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = 4; /// Allow up to 4 textures per material

	/// Create the descriptor pool
	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = 1; /// Just one set per material

	VkDescriptorPool pool;
	VK_CHECK(vkCreateDescriptorPool(this->device, &poolInfo, nullptr, &pool));

	this->descriptorPool = vulkan::VulkanDescriptorPoolHandle(
		pool,
		[this](VkDescriptorPool p) {
			vkDestroyDescriptorPool(this->device, p, nullptr);
		}
	);

	spdlog::debug("Created descriptor pool for material '{}'", this->name);

	return true;
}

VkDescriptorSetLayout Material::getDescriptorSetLayout() const {
	return this->descriptorSetLayout.get();
}

vulkan::PipelineConfig Material::getPipelineConfig() const {
	/// Start with default configuration for this material type
	auto config = this->getDefaultConfig();

	/// Configure vertex input state
	/// We use the Vertex struct's static methods to get consistent vertex layout
	config.setVertexInput(
		Vertex::getBindingDescription(),
		Vertex::getAttributeDescriptions()
	);

	/// Add shader stages from material's shader paths
	const auto shaderPaths = this->getShaderPaths();
	if (!shaderPaths.isValid()) {
		throw vulkan::VulkanException(
			VK_ERROR_INITIALIZATION_FAILED,
			"Invalid shader paths in material '" + this->name + "'",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	/// Add vertex and fragment shader stages
	config.addShaderStage(VK_SHADER_STAGE_VERTEX_BIT, shaderPaths.vertexPath);
	config.addShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, shaderPaths.fragmentPath);

	/// Allow derived classes to customize the configuration
	this->configurePipeline(config);

	return config;
}

vulkan::PipelineConfig Material::getDefaultConfig() const {
	vulkan::PipelineConfig config;

	/// Configure states based on material type and features
	/// We set these up in a specific order to ensure consistent state initialization
	this->initializeBlendState(config);
	this->initializeDepthState(config);
	this->initializeRasterizationState(config);

	spdlog::trace("Created default pipeline config for {} material '{}'",
		getMaterialTypeName(this->materialType), this->name);

	return config;
}

void Material::configurePipeline(vulkan::PipelineConfig& config) const {
	/// Base implementation provides no additional configuration
	/// Derived classes will override this to customize their pipeline settings
}

void Material::initializeBlendState(vulkan::PipelineConfig& config) const {
	/// Configure blending based on material features
	/// Transparent materials need alpha blending, while opaque materials don't
	if (this->hasFeature(MaterialFeatureFlags::Transparent)) {
		/// Set up alpha blending for transparent materials using standard blend factors
		/// This configuration is suitable for most transparent objects
		config.setBlendState(true,
			VK_BLEND_FACTOR_SRC_ALPHA,           /// src color blend factor
			VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, /// dst color blend factor
			VK_BLEND_OP_ADD,                     /// color blend op
			VK_BLEND_FACTOR_ONE,                 /// src alpha blend factor
			VK_BLEND_FACTOR_ZERO,                /// dst alpha blend factor
			VK_BLEND_OP_ADD                      /// alpha blend op
		);

		spdlog::trace("Configured blending for transparent material '{}'", this->name);
	} else {
		/// Disable blending for opaque materials to maximize performance
		/// This is the most efficient configuration for non-transparent objects
		config.setBlendState(false,
			VK_BLEND_FACTOR_ONE,  /// src color blend factor
			VK_BLEND_FACTOR_ZERO, /// dst color blend factor
			VK_BLEND_OP_ADD,      /// color blend op
			VK_BLEND_FACTOR_ONE,  /// src alpha blend factor
			VK_BLEND_FACTOR_ZERO, /// dst alpha blend factor
			VK_BLEND_OP_ADD       /// alpha blend op
		);
	}
}

void Material::initializeDepthState(vulkan::PipelineConfig& config) const {
	/// Configure depth testing based on material type
	/// Each type has specific depth requirements for correct rendering
	switch (this->materialType) {
		case MaterialType::Skybox:
			/// Skybox needs special depth handling:
			/// - Enable depth testing to ensure proper occlusion
			/// - Disable depth writing as skybox is infinitely far
			/// - Use LESS_OR_EQUAL to render at maximum depth
			config.setDepthState(true, false, VK_COMPARE_OP_LESS_OR_EQUAL);
			break;

		case MaterialType::Post:
			/// Post-processing materials operate in screen space:
			/// - Disable depth testing and writing
			/// - Always pass depth test to ensure full-screen effect
			config.setDepthState(false, false, VK_COMPARE_OP_ALWAYS);
			break;

		default:
			/// Standard materials use normal depth testing:
			/// - Enable depth testing and writing
			/// - Use GREATER for Reverse-Z configuration
			/// - This provides better depth precision
			config.setDepthState(true, true, VK_COMPARE_OP_GREATER);
			break;
	}

	spdlog::trace("Initialized depth state for {} material '{}'",
		getMaterialTypeName(this->materialType), this->name);
}

void Material::initializeRasterizationState(vulkan::PipelineConfig& config) const {
	/// Configure face culling based on material features

	VkCullModeFlags cullMode;
	switch (this->cullingMode) {
	case CullingMode::None:
		cullMode = VK_CULL_MODE_NONE;
		break;
	case CullingMode::Front:
		cullMode = VK_CULL_MODE_FRONT_BIT;
		break;
	case CullingMode::Back:
	default:
		cullMode = VK_CULL_MODE_BACK_BIT;
		break;
	}

	/// Double-sided materials need culling disabled
	if (this->hasFeature(MaterialFeatureFlags::DoubleSided))
		cullMode = VK_CULL_MODE_NONE;

	VkCullModeFlags cullMode = this->hasFeature(MaterialFeatureFlags::DoubleSided) ?
		VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;

	/// Set polygon mode based on material type
	/// Wireframe materials need line rendering instead of filled polygons
	VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
	if (this->materialType == MaterialType::Wireframe) {
		polygonMode = VK_POLYGON_MODE_LINE;
	}

	config.setRasterization(
		polygonMode,
		cullMode,
		VK_FRONT_FACE_COUNTER_CLOCKWISE  /// Standard counter-clockwise front face
	);

	spdlog::trace("Set rasterization state for {} material '{}' - cullMode: {}, polygonMode: {}",
		getMaterialTypeName(this->materialType), this->name, cullMode, polygonMode);
}

} /// namespace lillugsi::rendering