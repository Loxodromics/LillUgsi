#pragma once

#include <vulkan/vulkan.h>
#include <fmt/format.h>

/// Custom formatter for VkFormat
template <>
struct fmt::formatter<VkFormat> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(VkFormat format, FormatContext& ctx)
	{
		string_view name = "Unknown";
		switch (format)
		{
		case VK_FORMAT_D16_UNORM: name = "VK_FORMAT_D16_UNORM";
			break;
		case VK_FORMAT_D32_SFLOAT: name = "VK_FORMAT_D32_SFLOAT";
			break;
		case VK_FORMAT_D24_UNORM_S8_UINT: name = "VK_FORMAT_D24_UNORM_S8_UINT";
			break;
		default: break;
		}
		return formatter<string_view>::format(name, ctx);
	}
};

template <>
struct fmt::formatter<VkShaderStageFlagBits> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(VkShaderStageFlagBits stage, FormatContext& ctx)
	{
		string_view name = "Unknown";
		switch (stage)
		{
		case VK_SHADER_STAGE_VERTEX_BIT: name = "VERTEX";
			break;
		case VK_SHADER_STAGE_FRAGMENT_BIT: name = "FRAGMENT";
			break;
		case VK_SHADER_STAGE_COMPUTE_BIT: name = "COMPUTE";
			break;
		case VK_SHADER_STAGE_GEOMETRY_BIT: name = "GEOMETRY";
			break;
		default: break;
		}
		return formatter<string_view>::format(name, ctx);
	}
};

template <>
struct fmt::formatter<VkPrimitiveTopology> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(VkPrimitiveTopology topology, FormatContext& ctx)
	{
		string_view name = "Unknown";
		switch (topology)
		{
		case VK_PRIMITIVE_TOPOLOGY_POINT_LIST: name = "POINT_LIST";
			break;
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST: name = "LINE_LIST";
			break;
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP: name = "LINE_STRIP";
			break;
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST: name = "TRIANGLE_LIST";
			break;
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP: name = "TRIANGLE_STRIP";
			break;
		default: break;
		}
		return formatter<string_view>::format(name, ctx);
	}
};

template <>
struct fmt::formatter<VkPolygonMode> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(VkPolygonMode mode, FormatContext& ctx)
	{
		string_view name = "Unknown";
		switch (mode)
		{
		case VK_POLYGON_MODE_FILL: name = "FILL";
			break;
		case VK_POLYGON_MODE_LINE: name = "LINE";
			break;
		case VK_POLYGON_MODE_POINT: name = "POINT";
			break;
		default: break;
		}
		return formatter<string_view>::format(name, ctx);
	}
};

template <>
struct fmt::formatter<VkCullModeFlags> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(VkCullModeFlags mode, FormatContext& ctx)
	{
		string_view name = "Unknown";
		switch (mode)
		{
		case VK_CULL_MODE_NONE: name = "NONE";
			break;
		case VK_CULL_MODE_FRONT_BIT: name = "FRONT";
			break;
		case VK_CULL_MODE_BACK_BIT: name = "BACK";
			break;
		case VK_CULL_MODE_FRONT_AND_BACK: name = "FRONT_AND_BACK";
			break;
		default: break;
		}
		return formatter<string_view>::format(name, ctx);
	}
};

template <>
struct fmt::formatter<VkFrontFace> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(VkFrontFace face, FormatContext& ctx)
	{
		string_view name = "Unknown";
		switch (face)
		{
		case VK_FRONT_FACE_COUNTER_CLOCKWISE: name = "CCW";
			break;
		case VK_FRONT_FACE_CLOCKWISE: name = "CW";
			break;
		default: break;
		}
		return formatter<string_view>::format(name, ctx);
	}
};

template <>
struct fmt::formatter<VkCompareOp> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(VkCompareOp op, FormatContext& ctx)
	{
		string_view name = "Unknown";
		switch (op)
		{
		case VK_COMPARE_OP_NEVER: name = "NEVER";
			break;
		case VK_COMPARE_OP_LESS: name = "LESS";
			break;
		case VK_COMPARE_OP_EQUAL: name = "EQUAL";
			break;
		case VK_COMPARE_OP_LESS_OR_EQUAL: name = "LESS_OR_EQUAL";
			break;
		case VK_COMPARE_OP_GREATER: name = "GREATER";
			break;
		case VK_COMPARE_OP_NOT_EQUAL: name = "NOT_EQUAL";
			break;
		case VK_COMPARE_OP_GREATER_OR_EQUAL: name = "GREATER_OR_EQUAL";
			break;
		case VK_COMPARE_OP_ALWAYS: name = "ALWAYS";
			break;
		default: break;
		}
		return formatter<string_view>::format(name, ctx);
	}
};

template <>
struct fmt::formatter<VkBlendOp> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(VkBlendOp op, FormatContext& ctx)
	{
		string_view name = "Unknown";
		switch (op)
		{
		case VK_BLEND_OP_ADD: name = "ADD";
			break;
		case VK_BLEND_OP_SUBTRACT: name = "SUBTRACT";
			break;
		case VK_BLEND_OP_REVERSE_SUBTRACT: name = "REVERSE_SUBTRACT";
			break;
		case VK_BLEND_OP_MIN: name = "MIN";
			break;
		case VK_BLEND_OP_MAX: name = "MAX";
			break;
		default: break;
		}
		return formatter<string_view>::format(name, ctx);
	}
};
