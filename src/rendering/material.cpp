#include "pbrmaterial.h"
#include "vulkan/vulkanexception.h"
#include "vulkan/vulkanutils.h"
#include <spdlog/spdlog.h>

namespace lillugsi::rendering {

Material::Material(VkDevice device, const std::string& name, VkPhysicalDevice physicalDevice)
	: device(device)
	, physicalDevice(physicalDevice)
	, name(name) {

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
}

void Material::createDescriptorPool() {
	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSize.descriptorCount = 1;

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;
	poolInfo.maxSets = 1;

	VkDescriptorPool pool;
	VK_CHECK(vkCreateDescriptorPool(this->device, &poolInfo, nullptr, &pool));

	this->descriptorPool = vulkan::VulkanDescriptorPoolHandle(pool,
		[this](VkDescriptorPool p) {
			vkDestroyDescriptorPool(this->device, p, nullptr);
		});
}

VkDescriptorSetLayout Material::getDescriptorSetLayout() const {
	return this->descriptorSetLayout.get();
}

} /// namespace lillugsi::rendering