#pragma once

#include "boundingbox.h"
#include <array>

namespace lillugsi::scene {

/// Frustum class represents a view frustum for culling calculations
/// The frustum is defined by six planes that bound the visible volume
/// We use this for efficiently determining which objects are potentially visible
class Frustum {
public:
	/// A plane in 3D space, used to define the frustum boundaries
	struct Plane {
		glm::vec3 normal;	/// Plane normal vector
		float distance;		/// Distance from origin to plane

		/// Check if a point is in front of the plane
		/// @param point The point to test
		/// @return true if the point is in front of the plane
		bool isInFront(const glm::vec3& point) const;
	};

	/// Create a frustum from view and projection matrices
	/// @param viewProj Combined view-projection matrix
	static Frustum createFromMatrix(const glm::mat4& viewProj);

	/// Check if a point is inside the frustum
	/// @param point The point to test
	/// @return true if the point is inside the frustum
	bool containsPoint(const glm::vec3& point) const;

	/// Check if a bounding box intersects the frustum
	/// This is the main method used for frustum culling
	/// @param box The bounding box to test
	/// @return true if the box intersects the frustum
	bool intersectsBox(const BoundingBox& box) const;

	/// Get the frustum corners
	/// This is useful for visualization and detailed calculations
	/// @return Array of 8 corners of the frustum
	std::array<glm::vec3, 8> getCorners() const;

private:
	std::array<Plane, 6> planes;  /// The six planes defining the frustum

	/// Update the frustum planes from a matrix
	/// @param matrix The view-projection matrix
	void updatePlanes(const glm::mat4& matrix);
};

} /// namespace lillugsi::scene