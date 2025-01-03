#pragma once

#include "vulkan/vulkanwrappers.h"
#include "vulkan/vulkanexception.h"
#include "vulkan/shaderprogram.h"
#include "rendering/material.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace lillugsi::vulkan {

/// PipelineManager class
/// Responsible for creating and managing graphics pipelines and shader resources
/// Centralizes pipeline and shader creation to optimize resource usage
class PipelineManager {
public:
	/// Constructor
	/// @param device The logical Vulkan device
	/// @param renderPass The render pass with which the pipelines will be compatible
	PipelineManager(VkDevice device, VkRenderPass renderPass);

	/// Destructor
	~PipelineManager() = default;

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
		const std::string& name) const;

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

	VkDevice device;
	VkRenderPass renderPass;

	/// Global descriptor set layouts
	/// These are shared across all pipelines
	VulkanDescriptorSetLayoutHandle cameraDescriptorLayout;
	VulkanDescriptorSetLayoutHandle lightDescriptorLayout;

	/// Store pipelines and pipeline layouts as shared pointers
	/// This allows for shared ownership while keeping VulkanHandles move-only
	std::unordered_map<std::string, std::shared_ptr<VulkanPipelineHandle>> pipelines;
	std::unordered_map<std::string, std::shared_ptr<VulkanPipelineLayoutHandle>> pipelineLayouts;

	/// Cache for shader programs
	/// Key is generated from shader paths to enable reuse
	std::unordered_map<std::string, std::shared_ptr<ShaderProgram>> shaderPrograms;
};

} /// namespace lillugsi::vulkan