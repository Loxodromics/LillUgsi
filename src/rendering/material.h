#pragma once

#include "vulkan/vulkanwrappers.h"
#include "vulkan/pipelineconfig.h"
#include "materialtype.h"
#include "shadertype.h"
#include <string>

namespace lillugsi::rendering {

/// Base class for all materials in the rendering system
/// We use a pure virtual interface to allow for different material types
/// while maintaining a consistent interface for the renderer
class Material {
public:
	/// Virtual destructor ensures proper cleanup of derived classes
	virtual ~Material() = default;

	enum class CullingMode {
		None,
		Back,  /// Default for our engine
		Front  /// For models with inverted winding like glTF
	    };

	/// Get the shader paths for this material
	/// Pipeline manager uses this to create shader modules
	/// @return The shader paths configuration
	[[nodiscard]] virtual ShaderPaths getShaderPaths() const = 0;

	/// Get the pipeline configuration for this material
	/// Each material type can customize its pipeline settings while
	/// maintaining its base configuration from the material type
	/// @param device The logical device needed for shader module creation
	/// @return The pipeline configuration for this material
	[[nodiscard]] virtual vulkan::PipelineConfig getPipelineConfig() const;

	/// Get the type of this material
	/// This helps the renderer optimize drawing and state management
	/// @return The material's type
	[[nodiscard]] MaterialType getType() const { return this->materialType; }

	/// Get the feature flags for this material
	/// These flags determine which shader features are enabled
	/// @return The material's feature flags
	[[nodiscard]] MaterialFeatureFlags getFeatures() const { return this->features; }

	/// Get the name of this material
	/// This allows for material lookup and management
	/// @return The material's unique name
	[[nodiscard]] const std::string& getName() const { return this->name; }

	/// Check if a specific feature is enabled
	/// @param feature The feature to check
	/// @return True if the feature is enabled
	[[nodiscard]] bool hasFeature(MaterialFeatureFlags feature) const {
		return (this->features & feature) == feature;
	}

	void setCullingMode(CullingMode cullingMode) { this->cullingMode = cullingMode; }

	/// Bind this material's resources for rendering
	/// This includes setting up descriptor sets and push constants
	/// @param cmdBuffer The command buffer to record binding commands to
	/// @param pipelineLayout The pipeline layout for binding
	virtual void bind(VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout) const;

	/// Get the descriptor set layout for this material type
	/// Each material type can have its own unique set of descriptors
	/// @return The descriptor set layout for this material
	[[nodiscard]] virtual VkDescriptorSetLayout getDescriptorSetLayout() const;

	/// Disable copying to prevent multiple materials sharing GPU resources
	Material(const Material&) = delete;
	Material& operator=(const Material&) = delete;

	/// Define the texture channel enumeration for configuring texture sampling
	/// This allows us to flexibly specify which texture channels to use for each property
	enum class TextureChannel {
		R = 0,  /// Red channel
		G = 1,  /// Green channel
		B = 2,  /// Blue channel
		A = 3   /// Alpha channel
	    };

protected:
	/// Create a new Material
	/// @param device The logical device for resource creation
	/// @param name Unique name for this material instance
	/// @param physicalDevice The physical device for memory allocation
	/// @param type The type of material being created
	/// @param features Optional features to enable for this material
	Material(VkDevice device,
		const std::string& name,
		VkPhysicalDevice physicalDevice,
		MaterialType type = MaterialType::PBR,
		MaterialFeatureFlags features = MaterialFeatureFlags::None);

	/// Get the default pipeline configuration for this material type
	/// @return Default pipeline configuration for this material type
	[[nodiscard]] vulkan::PipelineConfig getDefaultConfig() const;

	/// Create a descriptor pool for material descriptors
	/// We create a dedicated pool for this material's descriptors
	/// to simplify resource management and lifetime
	/// Different material types may need different pool configurations
	/// @return True if the pool was created successfully
	virtual bool createDescriptorPool();

	/// Configure material-specific pipeline settings
	/// Derived classes should override this to customize their pipeline
	/// @param config The pipeline configuration to modify
	virtual void configurePipeline(vulkan::PipelineConfig& config) const;

	VkDevice device;                 /// Logical device reference
	VkPhysicalDevice physicalDevice; /// Physical device reference
	std::string name;                /// Unique material name
	MaterialType materialType;       /// Type of this material
	MaterialFeatureFlags features;   /// Enabled features for this material
	CullingMode cullingMode{CullingMode::Back}; /// Culling mode for the pipeline

protected:
	/// GPU resources managed by the material
	vulkan::VulkanDescriptorSetLayoutHandle descriptorSetLayout;
	vulkan::VulkanBufferHandle uniformBuffer;
	vulkan::VulkanDeviceMemoryHandle uniformBufferMemory;
	VkDescriptorSet descriptorSet;       /// Descriptor set for binding
	vulkan::VulkanDescriptorPoolHandle descriptorPool;

private:
	/// Initialize states based on material features
	void initializeBlendState(vulkan::PipelineConfig& config) const;
	void initializeDepthState(vulkan::PipelineConfig& config) const;
	void initializeRasterizationState(vulkan::PipelineConfig& config) const;
};

} /// namespace lillugsi::rendering