
#include "texture.h"
#include "vulkan/vulkanutils.h"
#include "vulkan/vulkanformatters.h"
#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>

namespace lillugsi::rendering {

Texture::Texture(VkDevice device, 
                 VkPhysicalDevice physicalDevice,
                 uint32_t width, 
                 uint32_t height, 
                 VkFormat format,
                 uint32_t mipLevels,
                 uint32_t layerCount,
                 const std::string& name)
	: device(device)
	, physicalDevice(physicalDevice)
	, width(width)
	, height(height)
	, layerCount(layerCount)
	, format(format)
	, name(name) {
	
	/// Calculate mipmap levels if automatic generation is requested (mipLevels = 0)
	/// This ensures we allocate the correct number of mip levels based on texture dimensions
	if (mipLevels == 0) {
		this->mipLevels = calculateMipLevels(width, height);
	} else {
		this->mipLevels = mipLevels;
	}
	
	/// Verify the requested format is supported
	/// This prevents attempting to create textures with unsupported formats
	if (!this->isFormatSupported(format, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
		throw vulkan::VulkanException(
			VK_ERROR_FORMAT_NOT_SUPPORTED,
			"Texture format " + std::to_string(format) + " is not supported for sampling",
			__FUNCTION__, __FILE__, __LINE__
		);
	}
	
	/// Create the image with the requested properties
	/// We use VK_IMAGE_USAGE_TRANSFER_DST_BIT to allow uploading data
	/// and VK_IMAGE_USAGE_TRANSFER_SRC_BIT to allow mipmap generation
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = this->mipLevels;
	imageInfo.arrayLayers = layerCount;
	imageInfo.format = format;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL; /// Optimal tiling for GPU access
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | 
	                  VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT; /// No multisampling for textures
	
	VkImage textureImage;
	VK_CHECK(vkCreateImage(device, &imageInfo, nullptr, &textureImage));
	
	/// Wrap in RAII handle for automatic cleanup
	this->image = vulkan::VulkanImageHandle(textureImage, [this](VkImage img) {
		vkDestroyImage(this->device, img, nullptr);
	});
	
	/// Allocate memory for the image
	/// Query the memory requirements for the image
	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device, this->image.get(), &memRequirements);
	
	/// Allocate device memory for the image
	/// For textures, we typically want device-local memory for optimal performance
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = this->findMemoryType(
		memRequirements.memoryTypeBits, 
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);
	
	VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &this->imageMemory));
	
	/// Bind the allocated memory to the image
	VK_CHECK(vkBindImageMemory(device, this->image.get(), this->imageMemory, 0));
	
	/// The image is created but still needs data to be uploaded
	/// The image starts in UNDEFINED layout
	this->currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	
	/// Log texture creation
	spdlog::info("Created texture {}x{} with {} mip levels, format {}", 
		width, height, this->mipLevels, format);
	
	/// Create image view
	/// The image view is needed for the shader to access the texture
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = this->image.get();
	viewInfo.viewType = layerCount == 6 ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = this->mipLevels;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = layerCount;
	
	VkImageView textureImageView;
	VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &textureImageView));
	
	/// Wrap in RAII handle for automatic cleanup
	this->imageView = vulkan::VulkanImageViewHandle(textureImageView, [this](VkImageView view) {
		vkDestroyImageView(this->device, view, nullptr);
	});
}

Texture::~Texture() {
	/// Free GPU resources
	/// The RAII handles will automatically clean up the image and image view
	/// We just need to handle the manually allocated memory
	if (this->imageMemory != VK_NULL_HANDLE) {
		vkFreeMemory(this->device, this->imageMemory, nullptr);
		this->imageMemory = VK_NULL_HANDLE;
	}
	
	spdlog::debug("Texture '{}' destroyed", this->name.empty() ? "unnamed" : this->name);
}

void Texture::uploadData(const void* data, size_t size, VkCommandPool commandPool, VkQueue queue) {
	/// Calculate expected data size to validate input
	/// This helps catch potential memory errors or mismatches
	VkDeviceSize imageSize;
	switch (this->format) {
	case VK_FORMAT_R8_UNORM:
		imageSize = static_cast<VkDeviceSize>(this->width * this->height * 1);
		break;
	case VK_FORMAT_R8G8_UNORM:
		imageSize = static_cast<VkDeviceSize>(this->width * this->height * 2);
		break;
	case VK_FORMAT_R8G8B8_SRGB:
		imageSize = static_cast<VkDeviceSize>(this->width * this->height * 3);
		break;
	case VK_FORMAT_R8G8B8A8_SRGB:
	case VK_FORMAT_R8G8B8A8_UNORM:
	case VK_FORMAT_B8G8R8A8_SRGB:
	case VK_FORMAT_B8G8R8A8_UNORM:
		imageSize = static_cast<VkDeviceSize>(this->width * this->height * 4);
		break;
	case VK_FORMAT_R16G16B16A16_SFLOAT:
		imageSize = static_cast<VkDeviceSize>(this->width * this->height * 8);
		break;
	case VK_FORMAT_R32G32B32A32_SFLOAT:
		imageSize = static_cast<VkDeviceSize>(this->width * this->height * 16);
		break;
	default:
		// For unknown formats, calculate based on pixel size
			imageSize = static_cast<VkDeviceSize>(this->width * this->height * 4); // Default to 4 bytes
		break;
	}
	
	/// Create a staging buffer to transfer data from CPU to GPU
	/// For optimal performance, we use a staging buffer rather than directly mapping image memory
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = imageSize;
	bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	
	VK_CHECK(vkCreateBuffer(this->device, &bufferInfo, nullptr, &stagingBuffer));
	
	/// Get memory requirements for the staging buffer
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(this->device, stagingBuffer, &memRequirements);
	
	/// Allocate host-visible memory for the staging buffer
	/// This allows CPU to write directly to the buffer memory
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = this->findMemoryType(
		memRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	);
	
	VK_CHECK(vkAllocateMemory(this->device, &allocInfo, nullptr, &stagingBufferMemory));
	VK_CHECK(vkBindBufferMemory(this->device, stagingBuffer, stagingBufferMemory, 0));
	
	/// Copy data to the staging buffer
	void* mapped;
	VK_CHECK(vkMapMemory(this->device, stagingBufferMemory, 0, imageSize, 0, &mapped));
	memcpy(mapped, data, static_cast<size_t>(imageSize));
	vkUnmapMemory(this->device, stagingBufferMemory);
	
	/// Prepare the image by transitioning to TRANSFER_DST_OPTIMAL layout
	/// This is required before copying data to the image
	this->transitionLayout(
		commandPool,
		queue,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	);
	
	/// Copy the staging buffer to the texture image
	VkCommandBuffer commandBuffer = this->beginSingleTimeCommands(commandPool);
	
	/// Set up copy region struct for the buffer-to-image copy
	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0; /// Tightly packed
	region.bufferImageHeight = 0; /// Tightly packed
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = this->layerCount;
	region.imageOffset = {0, 0, 0};
	region.imageExtent = {this->width, this->height, 1};
	
	/// Execute the copy command
	vkCmdCopyBufferToImage(
		commandBuffer,
		stagingBuffer,
		this->image.get(),
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&region
	);
	
	this->endSingleTimeCommands(commandBuffer, commandPool, queue);
	
	/// Clean up staging resources
	/// These are no longer needed after the copy is complete
	vkDestroyBuffer(this->device, stagingBuffer, nullptr);
	vkFreeMemory(this->device, stagingBufferMemory, nullptr);
	
	/// If the texture has mipmaps, generate them now
	/// Otherwise, transition directly to shader read layout
	if (this->mipLevels > 1) {
		this->generateMipmaps(commandPool, queue);
	} else {
		/// Transition to shader read layout if not generating mipmaps
		this->transitionLayout(
			commandPool,
			queue,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);
	}
	
	spdlog::info("Uploaded {} bytes of data to texture '{}'", 
		size, this->name.empty() ? "unnamed" : this->name);
}

void Texture::configureSampler(FilterMode minFilter, FilterMode magFilter,
                               WrapMode wrapU, WrapMode wrapV,
                               bool enableAnisotropy, float maxAnisotropy) {
	/// If sampler already exists, we need to destroy it first
	/// This allows reconfiguring an existing texture's sampling properties
	this->sampler.reset();
	
	/// Query physical device properties for sampler limits
	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(this->physicalDevice, &properties);
	
	/// Clamp anisotropy to device limits
	/// This prevents requesting a level of anisotropy that the device doesn't support
	if (enableAnisotropy) {
		maxAnisotropy = std::min(maxAnisotropy, properties.limits.maxSamplerAnisotropy);
	} else {
		maxAnisotropy = 1.0f; /// Disable anisotropy
	}
	
	/// Set up sampler create info
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = toVkFilter(magFilter);
	samplerInfo.minFilter = toVkFilter(minFilter);
	samplerInfo.addressModeU = toVkAddressMode(wrapU);
	samplerInfo.addressModeV = toVkAddressMode(wrapV);
	samplerInfo.addressModeW = toVkAddressMode(WrapMode::Repeat); /// W coordinate unused for 2D
	samplerInfo.anisotropyEnable = enableAnisotropy ? VK_TRUE : VK_FALSE;
	samplerInfo.maxAnisotropy = maxAnisotropy;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE; /// Use normalized [0,1] coordinates
	samplerInfo.compareEnable = VK_FALSE; /// Not a comparison sampler
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	
	/// Configure mipmapping
	/// If the texture has multiple mip levels, enable trilinear filtering
	if (this->mipLevels > 1) {
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR; /// Blend between mip levels
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = static_cast<float>(this->mipLevels);
		samplerInfo.mipLodBias = 0.0f; /// No bias in level-of-detail calculation
	} else {
		/// Disable mipmapping if texture only has one level
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 0.0f;
		samplerInfo.mipLodBias = 0.0f;
	}
	
	/// Create the sampler
	VkSampler textureSampler;
	VK_CHECK(vkCreateSampler(this->device, &samplerInfo, nullptr, &textureSampler));
	
	/// Wrap in RAII handle for automatic cleanup
	this->sampler = vulkan::VulkanSamplerHandle(textureSampler, [this](VkSampler s) {
		vkDestroySampler(this->device, s, nullptr);
	});
	
	this->hasSampler = true;
	
	spdlog::debug("Configured sampler for texture '{}' with min filter: {}, mag filter: {}", 
		this->name.empty() ? "unnamed" : this->name, 
		static_cast<int>(minFilter), static_cast<int>(magFilter));
}

void Texture::generateMipmaps(VkCommandPool commandPool, VkQueue queue) {
	/// Check if the format supports linear blitting
	/// Linear blitting is required for mipmap generation
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(this->physicalDevice, this->format, &formatProperties);
	
	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
		throw vulkan::VulkanException(
			VK_ERROR_FORMAT_NOT_SUPPORTED,
			"Texture format does not support linear blitting for mipmap generation",
			__FUNCTION__, __FILE__, __LINE__
		);
	}
	
	VkCommandBuffer commandBuffer = this->beginSingleTimeCommands(commandPool);
	
	/// Set up an image memory barrier to use for layout transitions
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = this->image.get();
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = this->layerCount;
	barrier.subresourceRange.levelCount = 1; /// One mip level at a time
	
	/// Calculate dimensions for each mip level
	/// For each level, we halve the dimensions until reaching 1x1
	int32_t mipWidth = static_cast<int32_t>(this->width);
	int32_t mipHeight = static_cast<int32_t>(this->height);
	
	/// Generate each mip level by blitting from the previous level
	for (uint32_t i = 1; i < this->mipLevels; i++) {
		/// Transition previous level to SRC layout for reading
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		
		vkCmdPipelineBarrier(
			commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);
		
		/// Set up the blit operation to generate this mip level
		VkImageBlit blit{};
		blit.srcOffsets[0] = {0, 0, 0};
		blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = this->layerCount;
		blit.dstOffsets[0] = {0, 0, 0};
		blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1, 
		                      mipHeight > 1 ? mipHeight / 2 : 1, 1};
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = this->layerCount;
		
		/// Execute the blit command
		/// VK_FILTER_LINEAR enables bilinear interpolation during downsampling
		vkCmdBlitImage(
			commandBuffer,
			this->image.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			this->image.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit,
			VK_FILTER_LINEAR
		);
		
		/// Transition previous level to SHADER_READ for final use
		/// This is done as we no longer need this level for blitting
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		
		vkCmdPipelineBarrier(
			commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);
		
		/// Update dimensions for next mip level
		/// Ensure we don't go below 1x1 pixels
		if (mipWidth > 1) mipWidth /= 2;
		if (mipHeight > 1) mipHeight /= 2;
	}
	/// Transition the last mip level to SHADER_READ layout
	/// This level was only transitioned to TRANSFER_DST but never to SHADER_READ
	barrier.subresourceRange.baseMipLevel = this->mipLevels - 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	this->endSingleTimeCommands(commandBuffer, commandPool, queue);

	/// Update the current layout to reflect the final state
	this->currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	this->mipmapsGenerated = true;

	spdlog::info("Generated {} mipmap levels for texture '{}'",
		this->mipLevels, this->name.empty() ? "unnamed" : this->name);
}

void Texture::transitionLayout(VkCommandPool commandPool,
                               VkQueue queue,
                               VkImageLayout oldLayout,
                               VkImageLayout newLayout,
                               uint32_t baseMipLevel,
                               uint32_t levelCount,
                               uint32_t baseArrayLayer,
                               uint32_t layerCount) {
	/// Early out if the image is already in the desired layout
	if (oldLayout == newLayout) {
		return;
	}

	VkCommandBuffer commandBuffer = this->beginSingleTimeCommands(commandPool);

	/// Set up an image memory barrier for the layout transition
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = this->image.get();
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = baseMipLevel;
	barrier.subresourceRange.levelCount = levelCount;
	barrier.subresourceRange.baseArrayLayer = baseArrayLayer;
	barrier.subresourceRange.layerCount = layerCount;

	/// Determine source and destination access masks and pipeline stages
	/// based on the layouts we're transitioning between
	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	/// Configure the barrier based on the specific transition
	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
	    newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		/// Transitioning from an undefined layout to a transfer destination
		/// This is used before copying data to the image
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	} else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
	           newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		/// Transitioning from a transfer destination to a shader readable layout
		/// This is done after copying data to the image
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	} else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
	           newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		/// Transitioning from shader readable to transfer destination
		/// This is used when updating an existing texture
		barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	} else {
		/// If the transition isn't one of the predefined ones, throw an error
		throw vulkan::VulkanException(
			VK_ERROR_VALIDATION_FAILED_EXT,
			"Unsupported layout transition from " + std::to_string(oldLayout) +
			" to " + std::to_string(newLayout),
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	/// Execute the pipeline barrier to perform the layout transition
	vkCmdPipelineBarrier(
		commandBuffer,
		sourceStage, destinationStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	this->endSingleTimeCommands(commandBuffer, commandPool, queue);

	/// Update the current layout to reflect the change
	this->currentLayout = newLayout;
}

VkCommandBuffer Texture::beginSingleTimeCommands(VkCommandPool commandPool, bool begin) const {
	/// Allocate a command buffer for the one-time operation
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	VK_CHECK(vkAllocateCommandBuffers(this->device, &allocInfo, &commandBuffer));

	/// Begin the command buffer if requested
	if (begin) {
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
	}

	return commandBuffer;
}

void Texture::endSingleTimeCommands(VkCommandBuffer commandBuffer,
                                    VkCommandPool commandPool,
                                    VkQueue queue) {
	/// End the command buffer recording
	VK_CHECK(vkEndCommandBuffer(commandBuffer));

	/// Submit the command buffer to the queue and wait for it to complete
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	/// Submit commands and wait for completion
	/// For one-time operations, we use a fence to ensure the operation completes
	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = 0;

	VkFence fence;
	VK_CHECK(vkCreateFence(this->device, &fenceInfo, nullptr, &fence));

	VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, fence));

	/// Wait for the fence to signal that command buffer execution has finished
	VK_CHECK(vkWaitForFences(this->device, 1, &fence, VK_TRUE, UINT64_MAX));

	/// Clean up the fence
	vkDestroyFence(this->device, fence, nullptr);

	/// Free the command buffer
	vkFreeCommandBuffers(this->device, commandPool, 1, &commandBuffer);
}

uint32_t Texture::calculateMipLevels(uint32_t width, uint32_t height) {
	/// Calculate the maximum number of mipmap levels based on the texture dimensions
	/// The formula is: floor(log2(max(width, height))) + 1
	/// This gives us a mip chain down to 1x1 pixels
	uint32_t maxDimension = std::max(width, height);
	uint32_t levels = static_cast<uint32_t>(std::floor(std::log2(maxDimension))) + 1;

	return levels;
}

uint32_t Texture::findMemoryType(uint32_t typeFilter,
                                 VkMemoryPropertyFlags properties) const {
	/// Use the utility function to find a suitable memory type
	return vulkan::utils::findMemoryType(this->physicalDevice, typeFilter, properties);
}

bool Texture::isFormatSupported(VkFormat format, VkFormatFeatureFlags featureFlags) const {
	/// Query the physical device for format properties
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(this->physicalDevice, format, &formatProperties);

	/// Check if the format supports the requested features with optimal tiling
	return (formatProperties.optimalTilingFeatures & featureFlags) == featureFlags;
}

bool Texture::hasAlpha() const {
	/// Determine if the texture format contains an alpha channel
	/// This affects blending and material transparency settings
	switch (this->format) {
		case VK_FORMAT_R8G8B8A8_UNORM:
		case VK_FORMAT_R8G8B8A8_SRGB:
		case VK_FORMAT_B8G8R8A8_UNORM:
		case VK_FORMAT_B8G8R8A8_SRGB:
		case VK_FORMAT_R16G16B16A16_SFLOAT:
		case VK_FORMAT_R32G32B32A32_SFLOAT:
			return true;
		default:
			return false;
	}
}

VkFilter Texture::toVkFilter(FilterMode mode) {
	/// Convert our filter mode enum to the corresponding Vulkan filter
	switch (mode) {
		case FilterMode::Nearest:
			return VK_FILTER_NEAREST;
		case FilterMode::Linear:
			return VK_FILTER_LINEAR;
		case FilterMode::Cubic:
			/// Note: Cubic filtering requires VK_IMG_filter_cubic extension
			/// For now, fall back to linear if cubic is requested
			return VK_FILTER_LINEAR;
		default:
			return VK_FILTER_LINEAR;
	}
}

VkSamplerAddressMode Texture::toVkAddressMode(WrapMode mode) {
	/// Convert our wrap mode enum to the corresponding Vulkan address mode
	switch (mode) {
		case WrapMode::Repeat:
			return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		case WrapMode::MirroredRepeat:
			return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		case WrapMode::ClampToEdge:
			return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		case WrapMode::ClampToBorder:
			return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		default:
			return VK_SAMPLER_ADDRESS_MODE_REPEAT;
	}
}

} /// namespace lillugsi::rendering