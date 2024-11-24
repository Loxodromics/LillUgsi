#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdint>

namespace lillugsi::scene {

/// NodeID is used to uniquely identify nodes in the scene
/// We use a 64-bit integer to ensure we don't run out of IDs even in large scenes
using NodeID = uint64_t;

/// Enum defining the type of bounds to use for scene nodes
/// Different bounds types offer different trade-offs between accuracy and performance
enum class BoundsType {
	Box,		/// Axis-aligned bounding box - fastest but least accurate
	Sphere,		/// Bounding sphere - good for rotating objects
	OBB		/// Oriented bounding box - most accurate but most expensive
};

/// Enum for visibility status of scene nodes
/// This helps track why a node is (in)visible and optimize culling
enum class VisibilityStatus {
	Unknown,	/// Initial state before visibility check
	Visible,	/// Node is visible in the current frame
	Culled,		/// Node was culled by frustum/occlusion
	OutOfRange	/// Node is too far for current LOD settings
};

/// Structure holding transform data for scene nodes
/// We store position, rotation, and scale separately to avoid
/// recomputing the full matrix when only one component changes
struct Transform {
	glm::vec3 position{0.0f};
	glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};  /// Identity quaternion
	glm::vec3 scale{1.0f};

	/// Combine the transform components into a single matrix
	/// This is called when we need the actual transform matrix for rendering
	/// @return The combined transformation matrix
	glm::mat4 toMatrix() const;

	/// Create a transform from a matrix
	/// This is useful when importing transforms from external sources
	/// @param matrix The transformation matrix to decompose
	/// @return A Transform structure with equivalent transformation
	static Transform fromMatrix(const glm::mat4& matrix);
};

} /// namespace lillugsi::scene