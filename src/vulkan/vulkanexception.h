#pragma once

#include <stdexcept>
#include <string>
#include <vulkan/vulkan.h>
#include <spdlog/spdlog.h>

class VulkanException : public std::runtime_error {
public:
	/// Constructor for VulkanException
	VulkanException(VkResult result, const std::string& message, const char* function, const char* file, int line)
		: std::runtime_error(formatMessage(result, message, function, file, line))
		, result(result)
		, function(function)
		, file(file)
		, line(line) {
		/// Log the exception details
		spdlog::error("VulkanException: {}", this->what());
	}

	/// Get the VkResult associated with this exception
	VkResult getResult() const { return this->result; }

	/// Get the function name where the exception occurred
	const std::string& getFunction() const { return this->function; }

	/// Get the file name where the exception occurred
	const std::string& getFile() const { return this->file; }

	/// Get the line number where the exception occurred
	int getLine() const { return this->line; }

private:
	VkResult result;
	std::string function;
	std::string file;
	int line;

	/// Format the error message
	static std::string formatMessage(VkResult result, const std::string& message, const char* function, const char* file, int line) {
		return "Vulkan error " + std::to_string(static_cast<int>(result)) +
			   " in " + function + " (" + file + ":" + std::to_string(line) + "): " + message +
			   " (" + string_VkResult(result) + ")";
	}

	/// Convert VkResult to string representation
	static const char* string_VkResult(VkResult result) {
		switch (result) {
			case VK_SUCCESS: return "VK_SUCCESS";
			case VK_NOT_READY: return "VK_NOT_READY";
			case VK_TIMEOUT: return "VK_TIMEOUT";
			case VK_EVENT_SET: return "VK_EVENT_SET";
			case VK_EVENT_RESET: return "VK_EVENT_RESET";
			case VK_INCOMPLETE: return "VK_INCOMPLETE";
			case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
			case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
			case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
			case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
			case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
			case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
			case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
			case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
			case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
			case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
			case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
			case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
			case VK_ERROR_UNKNOWN: return "VK_ERROR_UNKNOWN";
			case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
			case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
			case VK_ERROR_FRAGMENTATION: return "VK_ERROR_FRAGMENTATION";
			case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
			case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
			case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
			case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
			case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
			case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
			case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
			case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
			default: return "UNKNOWN_ERROR";
		}
	}
};

/// Macro for easy exception throwing
#define VK_CHECK(x) do { \
	VkResult err = x; \
	if (err) { \
		throw VulkanException(err, "Failed to perform " #x, __FUNCTION__, __FILE__, __LINE__); \
	} \
} while (0)