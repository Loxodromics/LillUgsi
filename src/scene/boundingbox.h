#pragma once

#include "scenetypes.h"
#include <array>

namespace lillugsi::scene {

/// BoundingBox class represents an axis-aligned bounding box (AABB)
/// We use AABBs for basic collision detection and frustum culling
/// AABBs are chosen for their simplicity and efficient intersection tests
class BoundingBox {
public:
	/// Create an empty bounding box
	/// An empty box is marked as invalid and will be updated when points are added
	BoundingBox();

	/// Create a bounding box from min and max points
	/// @param min The minimum point of the box
	/// @param max The maximum point of the box
	BoundingBox(const glm::vec3& min, const glm::vec3& max);

	/// Reset the bounding box to an invalid state
	/// This is used when we need to recompute the bounds from scratch
	void reset();

	/// Add a point to the bounding box
	/// The box will expand to contain the new point if necessary
	/// @param point The point to include in the bounds
	void addPoint(const glm::vec3& point);

	/// Transform the bounding box by a matrix
	/// This creates a new box that contains the transformed original box
	/// @param transform The transformation matrix to apply
	/// @return A new bounding box containing the transformed box
	BoundingBox transform(const glm::mat4& transform) const;

	/// Check if this box intersects another box
	/// @param other The other box to test against
	/// @return true if the boxes intersect
	bool intersects(const BoundingBox& other) const;

	/// Check if this box contains a point
	/// @param point The point to test
	/// @return true if the point is inside the box
	bool contains(const glm::vec3& point) const;

	/// Get the box corners
	/// This is useful for visualization and detailed intersection tests
	/// @return Array of 8 corners of the box
	std::array<glm::vec3, 8> getCorners() const;

	/// Get the center point of the box
	/// @return The center point
	glm::vec3 getCenter() const;

	/// Get the size of the box
	/// @return Vector from min to max point
	glm::vec3 getSize() const;

	/// Check if the box is valid
	/// @return true if the box has been initialized with valid points
	bool isValid() const { return this->valid; }

	/// Get the minimum point of the box
	/// @return The minimum point
	const glm::vec3& getMin() const { return this->min; }

	/// Get the maximum point of the box
	/// @return The maximum point
	const glm::vec3& getMax() const { return this->max; }

private:
	glm::vec3 min;	/// Minimum point of the box
	glm::vec3 max;	/// Maximum point of the box
	bool valid;	/// Indicates if the box contains valid data
};

} /// namespace lillugsi::scene