#include "screenshot.h"
#include "vulkan/vulkanexception.h"
#include "vulkan/vulkanutils.h"
#include "vulkan/vulkanformatters.h"
#include <spdlog/spdlog.h>

/// Include STB image write for saving PNG files
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace lillugsi::rendering {

Screenshot::Screenshot(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue, VkCommandPool commandPool)
	: device(device)
	, physicalDevice(physicalDevice)
	, queue(queue)
	, commandPool(commandPool) {
	spdlog::debug("Screenshot handler initialized");
}

Screenshot::~Screenshot() {
	this->cleanup();
}

bool Screenshot::captureScreenshot(
	VkImage swapchainImage,
	uint32_t width,
	uint32_t height,
	VkFormat format,
	const std::string& filename
) {
	try {
		/// Calculate the format size to allocate the right buffer size
		uint32_t formatSize = this->getFormatSize(format);
		if (formatSize == 0) {
			spdlog::error("Unsupported format for screenshot: {}", format);
			return false;
		}

		/// Create a buffer to hold the image data
		this->createScreenshotBuffer(width, height, formatSize);

		/// Allocate command buffer for transition and copy operations
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = this->commandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		VK_CHECK(vkAllocateCommandBuffers(this->device, &allocInfo, &commandBuffer));

		/// Begin command buffer recording
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

		/// Transition image to transfer source layout
		this->transitionImageLayout(
			swapchainImage,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, /// Assuming the image was just presented
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			commandBuffer
		);

		/// Copy image to buffer
		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0; /// Tightly packed
		region.bufferImageHeight = 0; /// Tightly packed
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = {0, 0, 0};
		region.imageExtent = {width, height, 1};

		vkCmdCopyImageToBuffer(
			commandBuffer,
			swapchainImage,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			this->screenshotBuffer.get(),
			1,
			&region
		);

		/// Transition image back to present layout
		this->transitionImageLayout(
			swapchainImage,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			commandBuffer
		);

		/// End command buffer recording
		VK_CHECK(vkEndCommandBuffer(commandBuffer));

		/// Submit the command buffer
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		/// Create a fence to wait for the command buffer to complete
		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

		VkFence fence;
		VK_CHECK(vkCreateFence(this->device, &fenceInfo, nullptr, &fence));

		/// Submit to the queue and wait
		VK_CHECK(vkQueueSubmit(this->queue, 1, &submitInfo, fence));
		VK_CHECK(vkWaitForFences(this->device, 1, &fence, VK_TRUE, UINT64_MAX));

		/// Clean up fence
		vkDestroyFence(this->device, fence, nullptr);

		/// Free the command buffer
		vkFreeCommandBuffers(this->device, this->commandPool, 1, &commandBuffer);

		/// Save the buffer data to a file
		bool success = this->saveScreenshotToPNG(width, height, filename);

		/// Log the result
		if (success) {
			spdlog::info("Screenshot saved to {}", filename);
		} else {
			spdlog::error("Failed to save screenshot to {}", filename);
		}

		return success;
	}
	catch (const vulkan::VulkanException& e) {
		spdlog::error("Vulkan error during screenshot capture: {}", e.what());
		return false;
	}
	catch (const std::exception& e) {
		spdlog::error("Error during screenshot capture: {}", e.what());
		return false;
	}
}

void Screenshot::transitionImageLayout(
	VkImage image,
	VkImageLayout oldLayout,
	VkImageLayout newLayout,
	VkCommandBuffer commandBuffer
) {
	/// Set up an image memory barrier to transition the image layout
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	/// Determine the pipeline stages and access masks based on the layouts
	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	if (oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
		/// Transitioning from present to transfer source
		/// Wait for presentation to finish before starting transfer operations
		barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
		/// Transitioning from transfer source back to present
		/// Wait for transfer operations to finish before presenting
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	}
	else {
		/// We only handle these specific transitions for screenshots
		throw vulkan::VulkanException(
			VK_ERROR_FORMAT_NOT_SUPPORTED,
			"Unsupported layout transition for screenshot",
			__FUNCTION__, __FILE__, __LINE__
		);
	}

	/// Record the barrier command
	vkCmdPipelineBarrier(
		commandBuffer,
		sourceStage, destinationStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);
}

void Screenshot::createScreenshotBuffer(uint32_t width, uint32_t height, uint32_t formatSize) {
	/// Clean up any existing buffer first
	if (this->screenshotBuffer) {
		this->cleanup();
	}

	/// Calculate the needed buffer size
	VkDeviceSize bufferSize = static_cast<VkDeviceSize>(width) * height * formatSize;
	this->screenshotBufferSize = bufferSize;

	/// Create the buffer with transfer destination usage
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = bufferSize;
	bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkBuffer buffer;
	VK_CHECK(vkCreateBuffer(this->device, &bufferInfo, nullptr, &buffer));

	/// Wrap the buffer in our RAII wrapper
	this->screenshotBuffer = vulkan::VulkanBufferHandle(buffer, [this](VkBuffer b) {
		vkDestroyBuffer(this->device, b, nullptr);
	});

	/// Get memory requirements
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(this->device, this->screenshotBuffer.get(), &memRequirements);

	/// Allocate memory for the buffer
	/// We need host visible memory so we can map it to read the data
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = vulkan::utils::findMemoryType(
		this->physicalDevice,
		memRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	);

	VK_CHECK(vkAllocateMemory(this->device, &allocInfo, nullptr, &this->screenshotBufferMemory));
	VK_CHECK(vkBindBufferMemory(this->device, this->screenshotBuffer.get(), this->screenshotBufferMemory, 0));

	spdlog::debug("Created screenshot buffer with size: {} bytes", bufferSize);
}

bool Screenshot::saveScreenshotToPNG(uint32_t width, uint32_t height, const std::string& filename) {
	/// Map the buffer memory so we can access the image data
	void* mappedMemory;
	VK_CHECK(vkMapMemory(this->device, this->screenshotBufferMemory, 0, this->screenshotBufferSize, 0, &mappedMemory));

	/// Create an RGBA buffer for the output image
	/// We need to convert from the swapchain format to RGBA for stb_image_write
	std::vector<unsigned char> rgbaData(width * height * 4);

	/// Convert from swapchain format to RGBA
	/// In this simple implementation, we assume B8G8R8A8 swapchain format
	/// More sophisticated conversion would be needed for other formats
	convertToRGBA(
		mappedMemory,
		rgbaData.data(),
		width,
		height,
		VK_FORMAT_B8G8R8A8_SRGB, /// Assumed format - would need to be passed from the swapchain
		4 /// Bytes per pixel
	);

	/// Unmap the memory
	vkUnmapMemory(this->device, this->screenshotBufferMemory);

	/// Save to PNG using stb_image_write
	/// The stride is the number of bytes per row (width * 4 for RGBA)
	/// We flip the image vertically because Vulkan and image file formats
	/// have different conventions for the origin (top-left vs bottom-left)
	int result = stbi_write_png(
		filename.c_str(),
		static_cast<int>(width),
		static_cast<int>(height),
		4, /// RGBA components
		rgbaData.data(),
		static_cast<int>(width * 4) /// Stride
	);

	return result != 0;
}

void Screenshot::cleanup() {
	/// Free the screenshot buffer memory and reset the handle
	if (this->screenshotBufferMemory != VK_NULL_HANDLE) {
		vkFreeMemory(this->device, this->screenshotBufferMemory, nullptr);
		this->screenshotBufferMemory = VK_NULL_HANDLE;
	}

	/// The buffer will be destroyed automatically through its RAII wrapper
	this->screenshotBuffer.reset();
	this->screenshotBufferSize = 0;
}

uint32_t Screenshot::getFormatSize(VkFormat format) const {
	/// Return the size in bytes for common formats
	switch (format) {
		case VK_FORMAT_R8G8B8A8_UNORM:
		case VK_FORMAT_R8G8B8A8_SRGB:
		case VK_FORMAT_B8G8R8A8_UNORM:
		case VK_FORMAT_B8G8R8A8_SRGB:
			return 4;

		case VK_FORMAT_R8G8B8_UNORM:
		case VK_FORMAT_R8G8B8_SRGB:
		case VK_FORMAT_B8G8R8_UNORM:
		case VK_FORMAT_B8G8R8_SRGB:
			return 3;

		case VK_FORMAT_R8G8_UNORM:
		case VK_FORMAT_R8G8_SRGB:
			return 2;

		case VK_FORMAT_R8_UNORM:
		case VK_FORMAT_R8_SRGB:
			return 1;

		default:
			spdlog::warn("Unknown format for screenshot, using default size of 4 bytes");
			return 4; /// Default to 4 bytes per pixel
	}
}

void Screenshot::convertToRGBA(
	const void* srcData,
	void* dstData,
	uint32_t width,
	uint32_t height,
	VkFormat format,
	uint32_t bytesPerPixel
) {
	const uint8_t* src = static_cast<const uint8_t*>(srcData);
	uint8_t* dst = static_cast<uint8_t*>(dstData);

	/// Convert based on format
	switch (format) {
		case VK_FORMAT_B8G8R8A8_UNORM:
		case VK_FORMAT_B8G8R8A8_SRGB:
			/// Convert from BGRA to RGBA
			for (uint32_t y = 0; y < height; ++y) {
				for (uint32_t x = 0; x < width; ++x) {
					uint32_t srcIdx = (y * width + x) * bytesPerPixel;
					uint32_t dstIdx = (y * width + x) * 4;

					/// Swap B and R channels
					dst[dstIdx + 0] = src[srcIdx + 2]; /// R <- B
					dst[dstIdx + 1] = src[srcIdx + 1]; /// G <- G
					dst[dstIdx + 2] = src[srcIdx + 0]; /// B <- R
					dst[dstIdx + 3] = src[srcIdx + 3]; /// A <- A
				}
			}
			break;

		case VK_FORMAT_R8G8B8A8_UNORM:
		case VK_FORMAT_R8G8B8A8_SRGB:
			/// Format already matches RGBA, just copy
			std::memcpy(dst, src, width * height * bytesPerPixel);
			break;

		default:
			/// For other formats, a more complex conversion would be needed
			/// For now, just do a simple copy and warn
			spdlog::warn("Format conversion not implemented for format: {}", format);
			std::memcpy(dst, src, width * height * bytesPerPixel);
			break;
	}

	/// Flip the image vertically since Vulkan and image formats have different origins
	/// This is a common approach for screenshots to match expected orientation
	std::vector<uint8_t> tempRow(width * 4);
	for (uint32_t y = 0; y < height / 2; ++y) {
		uint8_t* topRow = dst + y * width * 4;
		uint8_t* bottomRow = dst + (height - y - 1) * width * 4;

		/// Swap rows
		std::memcpy(tempRow.data(), topRow, width * 4);
		std::memcpy(topRow, bottomRow, width * 4);
		std::memcpy(bottomRow, tempRow.data(), width * 4);
	}
}

} // namespace lillugsi::rendering