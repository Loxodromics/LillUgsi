#include "scene/frustum.h"
#include <spdlog/spdlog.h>

namespace lillugsi::scene {

/// Static helper function to normalize a plane
/// @param plane The plane to normalize
/// @return The normalized plane
static Frustum::Plane normalizePlane(const Frustum::Plane& plane) {
	/// Calculate the length of the normal vector
	float length = glm::length(plane.normal);

	/// Return normalized plane
	/// We normalize both the normal vector and the distance
	return {
		plane.normal / length,
		plane.distance / length
	};
}

bool Frustum::Plane::isInFront(const glm::vec3& point) const {
	/// Calculate signed distance from point to plane
	/// If distance is positive, point is in front of plane
	/// We use dot product because the plane normal is normalized
	return glm::dot(this->normal, point) + this->distance > 0.0f;
}

Frustum Frustum::createFromMatrix(const glm::mat4& viewProj) {
	Frustum frustum;
	frustum.updatePlanes(viewProj);
	return frustum;
}

void Frustum::updatePlanes(const glm::mat4& matrix) {
	/// Extract planes from the view-projection matrix
	/// This is more efficient than constructing planes from frustum corners
	/// The method is based on the fact that each plane can be extracted
	/// directly from the rows and columns of the view-projection matrix

	/// Get the rows of the matrix for easier access
	/// We transpose the matrix because glm is column-major
	/// but we want to extract rows
	glm::mat4 transposed = glm::transpose(matrix);

	/// Left plane
	this->planes[0] = normalizePlane({
		glm::vec3(transposed[3] + transposed[0]), /// Add 4th row to 1st row
		transposed[3][3] + transposed[0][3]
	});

	/// Right plane
	this->planes[1] = normalizePlane({
		glm::vec3(transposed[3] - transposed[0]), /// Subtract 1st row from 4th row
		transposed[3][3] - transposed[0][3]
	});

	/// Bottom plane
	this->planes[2] = normalizePlane({
		glm::vec3(transposed[3] + transposed[1]), /// Add 4th row to 2nd row
		transposed[3][3] + transposed[1][3]
	});

	/// Top plane
	this->planes[3] = normalizePlane({
		glm::vec3(transposed[3] - transposed[1]), /// Subtract 2nd row from 4th row
		transposed[3][3] - transposed[1][3]
	});

	/// Near plane
	this->planes[4] = normalizePlane({
		glm::vec3(transposed[3] + transposed[2]), /// Add 4th row to 3rd row
		transposed[3][3] + transposed[2][3]
	});

	/// Far plane
	this->planes[5] = normalizePlane({
		glm::vec3(transposed[3] - transposed[2]), /// Subtract 3rd row from 4th row
		transposed[3][3] - transposed[2][3]
	});

	spdlog::trace("Frustum planes updated from view-projection matrix");
}

bool Frustum::containsPoint(const glm::vec3& point) const {
	/// Check if point is on the positive side of all planes
	/// A point is inside the frustum if it's in front of all planes
	for (const auto& plane : this->planes) {
		if (!plane.isInFront(point)) {
			return false; /// Point is behind at least one plane
		}
	}
	return true; /// Point is in front of all planes
}

bool Frustum::intersectsBox(const BoundingBox& box) const {
	/// If the box is invalid, it can't intersect
	if (!box.isValid()) {
		return false;
	}

	/// Get box corners only once to avoid recalculation
	auto corners = box.getCorners();

	/// Check each plane
	for (const auto& plane : this->planes) {
		/// Count corners behind the plane
		int cornersOutside = 0;

		for (const auto& corner : corners) {
			if (!plane.isInFront(corner)) {
				cornersOutside++;
			}
		}

		/// If all corners are behind the plane, the box is outside
		if (cornersOutside == 8) {
			return false;
		}
	}

	/// Box intersects or is inside frustum
	return true;
}

std::array<glm::vec3, 8> Frustum::getCorners() const {
	/// Calculate frustum corners by finding intersections of three planes
	/// This is useful for visualization and debugging
	std::array<glm::vec3, 8> corners;

	/// Helper lambda to find intersection of three planes
	auto intersectPlanes = [](const Plane& p1, const Plane& p2, const Plane& p3) -> glm::vec3 {
		/// Use Cramer's rule to solve the system of equations
		/// Each plane equation: normal.x * x + normal.y * y + normal.z * z + distance = 0
		glm::mat3 denomMat(p1.normal, p2.normal, p3.normal);
		float denom = glm::determinant(denomMat);

		/// Check for parallel planes
		if (std::abs(denom) < 1e-6f) {
			spdlog::warn("Parallel planes detected in frustum corner calculation");
			return glm::vec3(0.0f);
		}

		/// Calculate intersection point
		glm::vec3 b(-p1.distance, -p2.distance, -p3.distance);

		glm::mat3 mx(b, p2.normal, p3.normal);
		glm::mat3 my(p1.normal, b, p3.normal);
		glm::mat3 mz(p1.normal, p2.normal, b);

		return glm::vec3(
			glm::determinant(mx) / denom,
			glm::determinant(my) / denom,
			glm::determinant(mz) / denom
		);
	};

	/// Near corners (0-3)
	corners[0] = intersectPlanes(planes[4], planes[2], planes[0]); /// near-bottom-left
	corners[1] = intersectPlanes(planes[4], planes[2], planes[1]); /// near-bottom-right
	corners[2] = intersectPlanes(planes[4], planes[3], planes[1]); /// near-top-right
	corners[3] = intersectPlanes(planes[4], planes[3], planes[0]); /// near-top-left

	/// Far corners (4-7)
	corners[4] = intersectPlanes(planes[5], planes[2], planes[0]); /// far-bottom-left
	corners[5] = intersectPlanes(planes[5], planes[2], planes[1]); /// far-bottom-right
	corners[6] = intersectPlanes(planes[5], planes[3], planes[1]); /// far-top-right
	corners[7] = intersectPlanes(planes[5], planes[3], planes[0]); /// far-top-left

	return corners;
}

} /// namespace lillugsi::scene