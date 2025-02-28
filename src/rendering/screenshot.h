#pragma once

#include <string>
#include <vulkan/vulkan.h>
#include "vulkan/vulkanwrappers.h"

namespace lillugsi::rendering {

/// Handles capturing and saving screenshots from Vulkan render targets
/// This class encapsulates all screenshot functionality including buffer management,
/// image transitions, and file saving to simplify screenshot capture
class Screenshot {
public:
	/// Create a new screenshot handler
	/// @param device The logical device for creating resources
	/// @param physicalDevice The physical device for memory allocation
	/// @param queue The graphics queue for submitting commands
	/// @param commandPool The command pool for allocating command buffers
	Screenshot(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue, VkCommandPool commandPool);

	~Screenshot();

	/// Capture the current swapchain image to a file
	/// @param swapchainImage The image to capture
	/// @param width The width of the image
	/// @param height The height of the image
	/// @param format The format of the swapchain image
	/// @param filename The name of the file to save (PNG format)
	/// @return True if the screenshot was saved successfully
	[[nodiscard]] bool captureScreenshot(
		VkImage swapchainImage,
		uint32_t width,
		uint32_t height,
		VkFormat format,
		const std::string& filename
	);

private:
	/// Transition an image's layout to prepare for copy operations
	/// @param image The image to transition
	/// @param oldLayout The current layout of the image
	/// @param newLayout The desired layout of the image
	/// @param commandBuffer The command buffer to record the transition command
	void transitionImageLayout(
		VkImage image,
		VkImageLayout oldLayout,
		VkImageLayout newLayout,
		VkCommandBuffer commandBuffer
	);

	/// Create the buffer for storing screenshot data
	/// @param width The width of the image
	/// @param height The height of the image
	/// @param formatSize The size in bytes of each pixel
	void createScreenshotBuffer(uint32_t width, uint32_t height, uint32_t formatSize);

	/// Save the captured data to a PNG file
	/// @param width The width of the image
	/// @param height The height of the image
	/// @param filename The name of the file to save
	/// @return True if the file was saved successfully
	[[nodiscard]] bool saveScreenshotToPNG(uint32_t width, uint32_t height, const std::string& filename);

	/// Clean up screenshot resources to prevent memory leaks
	void cleanup();

	/// Get the size in bytes for a specific pixel format
	/// @param format The Vulkan format to check
	/// @return The size in bytes per pixel
	[[nodiscard]] uint32_t getFormatSize(VkFormat format) const;

	/// Convert swapchain format data to RGBA format for image libraries
	/// @param srcData Source data in swapchain format
	/// @param dstData Destination buffer for RGBA data
	/// @param width Image width
	/// @param height Image height
	/// @param format Swapchain image format
	/// @param bytesPerPixel Size of each pixel in the source format
	void convertToRGBA(
		const void* srcData,
		void* dstData,
		uint32_t width,
		uint32_t height,
		VkFormat format,
		uint32_t bytesPerPixel
	);

	VkDevice device;
	VkPhysicalDevice physicalDevice;
	VkQueue queue;
	VkCommandPool commandPool;

	vulkan::VulkanBufferHandle screenshotBuffer;
	VkDeviceMemory screenshotBufferMemory{VK_NULL_HANDLE};
	VkDeviceSize screenshotBufferSize{0};
};

} /// namespace lillugsi::rendering