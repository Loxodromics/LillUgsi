#include "scene/boundingbox.h"
#include <glm/gtx/transform.hpp>
#include <spdlog/spdlog.h>
#include <limits>

namespace lillugsi::scene {

BoundingBox::BoundingBox()
	/// Initialize to "invalid" state with reversed min/max
	/// This makes it easy to detect uninitialized boxes and
	/// allows for proper expansion with the first point
	: min(std::numeric_limits<float>::max())
	, max(-std::numeric_limits<float>::max())
	, valid(false) {
}

BoundingBox::BoundingBox(const glm::vec3& min, const glm::vec3& max)
	: min(min)
	, max(max)
	, valid(true) {
	/// Validate that min is actually less than max
	/// This ensures the box is properly formed
	if (min.x > max.x || min.y > max.y || min.z > max.z) {
		spdlog::warn("Creating bounding box with min > max, swapping coordinates");
		/// Swap the coordinates if they're in the wrong order
		this->min = glm::min(min, max);
		this->max = glm::max(min, max);
	}
}

void BoundingBox::reset() {
	/// Reset to initial invalid state
	/// This is used when we need to recompute bounds from scratch
	this->min = glm::vec3(std::numeric_limits<float>::max());
	this->max = glm::vec3(-std::numeric_limits<float>::max());
	this->valid = false;
}

void BoundingBox::addPoint(const glm::vec3& point) {
	/// Update minimum and maximum points
	/// We use component-wise min/max to ensure each dimension is properly bounded
	this->min = glm::min(this->min, point);
	this->max = glm::max(this->max, point);
	this->valid = true;

	spdlog::trace("Added point ({}, {}, {}) to bounding box, new bounds: ({}, {}, {}) to ({}, {}, {})",
		point.x, point.y, point.z,
		this->min.x, this->min.y, this->min.z,
		this->max.x, this->max.y, this->max.z);
}

BoundingBox BoundingBox::transform(const glm::mat4& transform) const {
	/// If the box is invalid, return an invalid box
	if (!this->valid) {
		spdlog::warn("Attempting to transform invalid bounding box");
		return BoundingBox();
	}

	/// Check for extreme scaling in the transform matrix
	/// Extract scale components from the matrix (from columns 0-2)
	float scaleX = glm::length(glm::vec3(transform[0]));
	float scaleY = glm::length(glm::vec3(transform[1]));
	float scaleZ = glm::length(glm::vec3(transform[2]));

	if (scaleX < 1e-6f || scaleY < 1e-6f || scaleZ < 1e-6f) {
		spdlog::warn("Transform contains extremely small scale, clamping: ({}, {}, {})",
			scaleX, scaleY, scaleZ);
		// Return a small box around the transform's position
		glm::vec3 position(transform[3]);
		return BoundingBox(position - glm::vec3(0.01f), position + glm::vec3(0.01f));
	}

	/// Transform all 8 corners of the box
	/// This ensures we get a proper bounding box that contains
	/// the entire transformed original box
	auto corners = this->getCorners();

	/// Start with the first transformed point
	glm::vec4 transformedCorner = transform * glm::vec4(corners[0], 1.0f);
	glm::vec3 newMin = glm::vec3(transformedCorner);
	glm::vec3 newMax = newMin;

	/// Transform remaining corners and expand bounds
	for (size_t i = 1; i < corners.size(); ++i) {
		transformedCorner = transform * glm::vec4(corners[i], 1.0f);
		glm::vec3 transformedPoint = glm::vec3(transformedCorner);
		newMin = glm::min(newMin, transformedPoint);
		newMax = glm::max(newMax, transformedPoint);
	}

	return BoundingBox(newMin, newMax);
}

bool BoundingBox::intersects(const BoundingBox& other) const {
	/// If either box is invalid, they don't intersect
	if (!this->valid || !other.valid) {
		return false;
	}

	/// Check for overlap in all three dimensions
	/// Boxes intersect if they overlap on all axes
	bool overlapX = this->max.x >= other.min.x && this->min.x <= other.max.x;
	bool overlapY = this->max.y >= other.min.y && this->min.y <= other.max.y;
	bool overlapZ = this->max.z >= other.min.z && this->min.z <= other.max.z;

	return overlapX && overlapY && overlapZ;
}

bool BoundingBox::contains(const glm::vec3& point) const {
	/// If the box is invalid, it contains nothing
	if (!this->valid) {
		return false;
	}

	/// Check if the point is within bounds on all axes
	return point.x >= this->min.x && point.x <= this->max.x &&
		   point.y >= this->min.y && point.y <= this->max.y &&
		   point.z >= this->min.z && point.z <= this->max.z;
}

std::array<glm::vec3, 8> BoundingBox::getCorners() const {
	/// Return all 8 corners of the box
	/// The corners are arranged in this order:
	/// 0: min.x, min.y, min.z
	/// 1: max.x, min.y, min.z
	/// 2: max.x, max.y, min.z
	/// 3: min.x, max.y, min.z
	/// 4: min.x, min.y, max.z
	/// 5: max.x, min.y, max.z
	/// 6: max.x, max.y, max.z
	/// 7: min.x, max.y, max.z
	return {{
		glm::vec3(this->min.x, this->min.y, this->min.z), /// Bottom face
		glm::vec3(this->max.x, this->min.y, this->min.z),
		glm::vec3(this->max.x, this->max.y, this->min.z),
		glm::vec3(this->min.x, this->max.y, this->min.z),
		glm::vec3(this->min.x, this->min.y, this->max.z), /// Top face
		glm::vec3(this->max.x, this->min.y, this->max.z),
		glm::vec3(this->max.x, this->max.y, this->max.z),
		glm::vec3(this->min.x, this->max.y, this->max.z)
	}};
}

glm::vec3 BoundingBox::getCenter() const {
	/// Return the center point of the box
	/// For an invalid box, return zero
	if (!this->valid) {
		return glm::vec3(0.0f);
	}

	/// Center is halfway between min and max points
	return (this->min + this->max) * 0.5f;
}

glm::vec3 BoundingBox::getSize() const {
	/// Return the size of the box
	/// For an invalid box, return zero
	if (!this->valid) {
		return glm::vec3(0.0f);
	}

	/// Size is the difference between max and min points
	return this->max - this->min;
}

} /// namespace lillugsi::scene