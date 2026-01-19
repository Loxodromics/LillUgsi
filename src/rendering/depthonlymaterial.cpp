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

	/// Depth state: test and write enabled, LESS for Normal Z
	config.setDepthState(true, true, VK_COMPARE_OP_LESS);

	spdlog::trace("Configured depth-only pipeline for subpass 0");
}

} /// namespace lillugsi::rendering
