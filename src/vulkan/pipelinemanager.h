#pragma once

#include "vulkan/vulkanwrappers.h"
#include "vulkan/vulkanexception.h"
#include "vulkan/shaderprogram.h"
#include "rendering/material.h"

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace lillugsi::rendering {
	class DepthOnlyMaterial;
}

namespace lillugsi::vulkan {

/// PipelineManager class
/// Responsible for creating and managing graphics pipelines and shader resources
///
/// PipelineManager centralizes pipeline creation and management in the rendering system.
/// It optimizes Vulkan pipeline usage by sharing pipelines between materials with identical
/// configurations while maintaining separate uniform data.
///
/// Key Features:
/// - Pipeline sharing based on configuration hashes
/// - RAII resource management through smart pointers
/// - Reference counting for shared pipeline resources
/// - Separation of pipeline configuration from material properties
///
/// Usage Flow:
/// 1. Material defines its configuration (shaders, states, etc.)
/// 2. PipelineManager creates or reuses matching pipeline
/// 3. Material maintains unique uniforms while sharing pipeline
///
/// Resource Management:
/// - PipelineCache: Stores shared Vulkan pipelines and layouts
/// - MaterialPipeline: Material-specific RAII handles for shared resources
/// - Cleanup happens automatically through reference counting
///
/// Example:
/// Multiple PBR materials share one pipeline while having different colors,
/// metallic values, etc. This minimizes pipeline creation overhead while
/// maintaining material flexibility.

class PipelineManager {
	public:
		/// Constructor
		/// @param device The logical Vulkan device
		/// @param physicalDevice The physical Vulkan device
		/// @param renderPass The render pass with which the pipelines will be compatible
	PipelineManager(VkDevice device, VkPhysicalDevice physicalDevice, VkRenderPass renderPass);

	/// Destructor - defined in .cpp for complete type
	~PipelineManager();

	/// Initialize global descriptor layouts
	/// Must be called before any pipeline creation
	void initialize();

	/// Create a pipeline for a material
	/// This handles both shader creation and pipeline configuration
	/// @param material The material to create a pipeline for
	/// @return A shared pointer to the created pipeline handle
	[[nodiscard]] std::shared_ptr<VulkanPipelineHandle> createPipeline(
		const rendering::Material& material);

	/// Get a pipeline by material name
	/// @param name The name of the pipeline to retrieve
	/// @return A shared pointer to the requested pipeline handle, or nullptr if not found
	[[nodiscard]] std::shared_ptr<VulkanPipelineHandle> getPipeline(
		const std::string& name);

	/// Get a pipeline layout by material name
	/// @param name The name of the pipeline layout to retrieve
	/// @return A shared pointer to the requested pipeline layout handle
	[[nodiscard]] std::shared_ptr<VulkanPipelineLayoutHandle> getPipelineLayout(
		const std::string& name) const;

	/// Get the camera descriptor set layout
	/// Used for view and projection matrices (set = 0)
	/// @return The global camera descriptor layout
	[[nodiscard]] VkDescriptorSetLayout getCameraDescriptorLayout() const {
		return this->cameraDescriptorLayout.get();
	}

	/// Get the light descriptor set layout
	/// Used for light data (set = 1)
	/// @return The global light descriptor layout
	[[nodiscard]] VkDescriptorSetLayout getLightDescriptorLayout() const {
		return this->lightDescriptorLayout.get();
	}

	/// Get the depth-only pipeline layout
	/// Used for depth pre-pass rendering (only camera set)
	/// @return The depth-only pipeline layout
	[[nodiscard]] VkPipelineLayout getDepthPipelineLayout() const {
		return this->depthPipelineLayout.get();
	}

	/// Get or create a depth-only pipeline for the given material
	/// Depth pipelines use a simplified layout (camera only)
	/// @param material The material to create a depth pipeline for
	/// @return A shared pointer to the depth pipeline handle
	[[nodiscard]] std::shared_ptr<VulkanPipelineHandle> getOrCreateDepthPipeline(
		const rendering::Material& material);

	/// Check if a pipeline exists for a material
	/// This is needed for the PipelineFactory to avoid creating duplicate pipelines
	/// and for efficient resource management during model loading
	/// @param materialName The name of the material to check for
	/// @return True if a pipeline exists for this material
	[[nodiscard]] bool hasPipeline(const std::string& materialName) const;

	/// Create a compute pipeline with a descriptor layout
	/// @param name Unique name for this compute pipeline
	/// @param shaderPath Path to the compute shader SPIR-V file
	/// @param descriptorLayout Descriptor set layout for compute resources
	/// @return A shared pointer to the created compute pipeline handle
	[[nodiscard]] std::shared_ptr<VulkanPipelineHandle> createComputePipeline(
		const std::string& name,
		const std::string& shaderPath,
		VkDescriptorSetLayout descriptorLayout);

	/// Get a compute pipeline by name
	/// @param name The name of the compute pipeline to retrieve
	/// @return A shared pointer to the requested compute pipeline handle, or nullptr if not found
	[[nodiscard]] std::shared_ptr<VulkanPipelineHandle> getComputePipeline(
		const std::string& name);

	/// Get a compute pipeline layout by name
	/// @param name The name of the compute pipeline layout to retrieve
	/// @return A shared pointer to the requested compute pipeline layout handle
	[[nodiscard]] std::shared_ptr<VulkanPipelineLayoutHandle> getComputePipelineLayout(
		const std::string& name) const;

	/// Remove a compute pipeline from the cache
	/// @param name The name of the compute pipeline to remove
	void removeComputePipeline(const std::string& name);

	/// Clean up all pipelines and shader resources
	void cleanup();

private:
	/// Create shader program for a material
	/// @param paths Shader paths from the material
	/// @return A shared pointer to the created shader program
	[[nodiscard]] std::shared_ptr<ShaderProgram> createShaderProgram(
		const rendering::ShaderPaths& paths);

	/// Get or create shader program for given paths
	/// This implements shader program caching
	/// @param paths Shader paths to look up or create program for
	/// @return A shared pointer to the shader program
	[[nodiscard]] std::shared_ptr<ShaderProgram> getOrCreateShaderProgram(
		const rendering::ShaderPaths& paths);

	/// Generate a unique key for shader program caching
	/// @param paths The shader paths to generate a key for
	/// @return A string key combining the shader paths
	[[nodiscard]] static std::string generateShaderKey(
		const rendering::ShaderPaths& paths);

	/// Create the global descriptor set layouts
	/// These layouts are used by all materials
	void createGlobalDescriptorLayouts();

	/// Cache structure for shared pipeline resources
	/// Multiple materials can share the same underlying pipeline and layout
	/// while maintaining their own RAII handles
	struct PipelineCache {
		VkPipeline pipeline = VK_NULL_HANDLE;         /// Raw pipeline handle for sharing
		VkPipelineLayout layout = VK_NULL_HANDLE;     /// Raw layout handle for sharing
		uint32_t referenceCount{0};  /// Track number of materials using this pipeline
	};

	/// Pipeline handles for a specific material
	/// Each material gets its own RAII handles even when sharing pipelines
	struct MaterialPipeline {
		std::shared_ptr<VulkanPipelineHandle> pipeline;
		std::shared_ptr<VulkanPipelineLayoutHandle> layout;
	};

	/// Get or create pipeline for a material
	/// @param config Pipeline configuration from the material
	/// @param material The material requesting the pipeline
	/// @return Material-specific pipeline handles
	[[nodiscard]] MaterialPipeline getOrCreatePipeline(
		PipelineConfig& config,
		const rendering::Material& material);

	VkDevice device;
	VkPhysicalDevice physicalDevice;
	VkRenderPass renderPass;

	/// Shared depth-only material for all depth pipelines
	/// This provides the correct pipeline config (subpass 0, depth shaders)
	std::unique_ptr<rendering::DepthOnlyMaterial> depthMaterial;

	/// Cache of shared pipeline resources by configuration
	/// Multiple materials with the same configuration share these pipelines
	std::unordered_map<size_t, PipelineCache> pipelinesByConfig;

	/// Material-specific pipeline handles
	/// Each material gets its own entry even when sharing pipelines
	std::unordered_map<std::string, MaterialPipeline> materialPipelines;

	/// Global descriptor set layouts
	/// These are shared across all pipelines
	VulkanDescriptorSetLayoutHandle cameraDescriptorLayout;
	VulkanDescriptorSetLayoutHandle lightDescriptorLayout;

	/// Depth-only pipeline layout (camera set only)
	/// Used for depth pre-pass rendering
	VulkanPipelineLayoutHandle depthPipelineLayout;

	/// Storage for depth-only pipelines
	/// Each material type gets its own depth pipeline
	std::unordered_map<size_t, MaterialPipeline> depthPipelines;

	/// Named pipelines for direct lookup
	/// We keep this for compatibility and explicit pipeline access
	std::unordered_map<std::string, std::shared_ptr<VulkanPipelineHandle>> pipelines;
	std::unordered_map<std::string, std::shared_ptr<VulkanPipelineLayoutHandle>> pipelineLayouts;

	/// Cache for shader programs
	/// Key is generated from shader paths to enable reuse
	std::unordered_map<std::string, std::shared_ptr<ShaderProgram>> shaderPrograms;

	/// Set of materials we've already warned about
	/// Prevents log spam for missing materials
	mutable std::unordered_set<std::string> missingPipelineWarnings;

	/// Compute pipeline storage
	/// Separate from graphics pipelines for clarity
	std::unordered_map<std::string, MaterialPipeline> computePipelines;

	/// Track if cleanup has been called
	bool isCleanedUp{false};
};

} /// namespace lillugsi::vulkan