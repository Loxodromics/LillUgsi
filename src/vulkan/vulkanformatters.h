#pragma once

#include <vulkan/vulkan.h>
#include <fmt/format.h>

/// Custom formatter for VkFormat
/// This allows spdlog to properly format VkFormat values in log messages
template <>
struct fmt::formatter<VkFormat> : formatter<string_view> {
	/// Format the VkFormat value
	/// @param format The VkFormat value to format
	/// @param ctx The format context
	/// @return The formatted string
	template <typename FormatContext>
	auto format(VkFormat format, FormatContext& ctx) {
		string_view name = "Unknown";
		switch (format) {
		case VK_FORMAT_D16_UNORM: name = "VK_FORMAT_D16_UNORM"; break;
		case VK_FORMAT_D32_SFLOAT: name = "VK_FORMAT_D32_SFLOAT"; break;
		case VK_FORMAT_D24_UNORM_S8_UINT: name = "VK_FORMAT_D24_UNORM_S8_UINT"; break;
			/// Add more cases as needed
		default:
			break;
		}
		return formatter<string_view>::format(name, ctx);
	}
};