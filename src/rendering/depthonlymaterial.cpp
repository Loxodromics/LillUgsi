#include "depthonlymaterial.h"
#include "vulkan/pipelineconfig.h"
#include <spdlog/spdlog.h>

namespace lillugsi::rendering {

DepthOnlyMaterial::DepthOnlyMaterial(VkDevice device,
	const std::string& name,
	VkPhysicalDevice physicalDevice)
	: Material(device, name, physicalDevice, MaterialType::DepthOnly, MaterialFeatureFlags::None) {
	spdlog::debug("Created depth-only material: {}", name);
}

ShaderPaths DepthOnlyMaterial::getShaderPaths() const {
	return ShaderPaths{
		"shaders/depth.vert.spv",
		"shaders/depth.frag.spv"
	};
}

void DepthOnlyMaterial::configurePipeline(vulkan::PipelineConfig& config) const {
	/// Target subpass 0 (depth pre-pass)
	config.setSubpassIndex(0);

	/// Depth state: test and write enabled, GREATER for Reverse-Z
	/// Near objects have higher depth values (~1.0), far objects have lower (~0.0)
	config.setDepthState(true, true, VK_COMPARE_OP_GREATER);

	spdlog::trace("Configured depth-only pipeline for subpass 0");
}

} /// namespace lillugsi::rendering
