#pragma once

#include "material.h"

namespace lillugsi::rendering {

/// DepthOnlyMaterial is specialized for depth pre-pass rendering
/// It uses minimal shaders that only output depth values
/// This material targets subpass 0 which has no color attachments
class DepthOnlyMaterial : public Material {
public:
	DepthOnlyMaterial(VkDevice device,
		const std::string& name,
		VkPhysicalDevice physicalDevice);

	~DepthOnlyMaterial() override = default;

	/// Get the shader paths for depth-only rendering
	[[nodiscard]] ShaderPaths getShaderPaths() const override;

protected:
	/// Configure the pipeline for depth-only rendering
	/// Sets subpass index to 0 (depth pre-pass)
	void configurePipeline(vulkan::PipelineConfig& config) const override;
};

} /// namespace lillugsi::rendering
