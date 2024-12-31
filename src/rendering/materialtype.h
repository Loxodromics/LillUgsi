#pragma once

#include <cstdint>
#include <string_view>

namespace lillugsi::rendering {

/// MaterialType defines the categories of materials supported by the renderer
/// We use an enum class for type safety and to prevent implicit conversions
/// Each type determines:
/// - Which pipeline configuration to use
/// - What shader features are available
/// - Default render states and parameters
/// - Optimization hints for the renderer
enum class MaterialType : uint32_t {
	/// Standard material types for common rendering cases
	PBR,            /// Physically based rendering material
	Unlit,          /// Simple unlit material, no lighting calculations
	Basic,          /// Basic lit material, simplified lighting

	/// Technical materials for development and debugging
	Debug,          /// Visualization material for debugging
	Wireframe,      /// Wireframe rendering material
	Normals,        /// Normal vector visualization

	/// Special purpose materials
	Skybox,         /// Specialized material for skybox rendering
	Post,           /// Post-processing material
	Custom          /// User-defined material type
};

/// Get string representation of material type
/// This helps with logging and debugging
/// @param type The material type to convert
/// @return String view of the material type name
[[nodiscard]] constexpr std::string_view getMaterialTypeName(MaterialType type) {
	switch (type) {
		case MaterialType::PBR:       return "PBR";
		case MaterialType::Unlit:     return "Unlit";
		case MaterialType::Basic:     return "Basic";
		case MaterialType::Debug:     return "Debug";
		case MaterialType::Wireframe: return "Wireframe";
		case MaterialType::Normals:   return "Normals";
		case MaterialType::Skybox:    return "Skybox";
		case MaterialType::Post:      return "Post";
		case MaterialType::Custom:    return "Custom";
		default:                      return "Unknown";
	}
}

/// MaterialFeatureFlags defines optional features that can be enabled for materials
/// We use flags to allow combinations of features to be enabled
/// This helps optimize shader compilation by only including needed features
enum class MaterialFeatureFlags : uint32_t {
	None          = 0,
	Textured      = 1 << 0,  /// Material uses textures
	Transparent   = 1 << 1,  /// Material requires transparency
	DoubleSided   = 1 << 2,  /// Material is rendered on both sides
	VertexColor   = 1 << 3,  /// Material uses vertex colors
	Instanced     = 1 << 4,  /// Material supports instanced rendering
	ReceiveShadow = 1 << 5,  /// Material receives shadows
	CastShadow    = 1 << 6,  /// Material casts shadows
	Skinned       = 1 << 7,  /// Material supports skinned mesh rendering
};

/// Enable bitwise operations for MaterialFeatureFlags
/// This allows us to combine flags using natural syntax like:
/// auto flags = MaterialFeatureFlags::Textured | MaterialFeatureFlags::Transparent
inline constexpr MaterialFeatureFlags operator|(MaterialFeatureFlags a, MaterialFeatureFlags b) {
	return static_cast<MaterialFeatureFlags>(
		static_cast<uint32_t>(a) | static_cast<uint32_t>(b)
	);
}

inline constexpr MaterialFeatureFlags operator&(MaterialFeatureFlags a, MaterialFeatureFlags b) {
	return static_cast<MaterialFeatureFlags>(
		static_cast<uint32_t>(a) & static_cast<uint32_t>(b)
	);
}

inline constexpr MaterialFeatureFlags operator^(MaterialFeatureFlags a, MaterialFeatureFlags b) {
	return static_cast<MaterialFeatureFlags>(
		static_cast<uint32_t>(a) ^ static_cast<uint32_t>(b)
	);
}

inline constexpr MaterialFeatureFlags& operator|=(MaterialFeatureFlags& a, MaterialFeatureFlags b) {
	return a = a | b;
}

inline constexpr MaterialFeatureFlags& operator&=(MaterialFeatureFlags& a, MaterialFeatureFlags b) {
	return a = a & b;
}

inline constexpr MaterialFeatureFlags& operator^=(MaterialFeatureFlags& a, MaterialFeatureFlags b) {
	return a = a ^ b;
}

} /// namespace lillugsi::rendering