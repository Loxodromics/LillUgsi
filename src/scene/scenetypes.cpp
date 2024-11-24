#include "scene/scenetypes.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <spdlog/spdlog.h>

namespace lillugsi::scene {

glm::mat4 Transform::toMatrix() const {
	/// Start with identity matrix as our base
	/// We build the transformation matrix in the standard order:
	/// scale -> rotate -> translate
	/// This order is important as matrix multiplication is not commutative
	glm::mat4 transform{1.0f};

	/// Apply translation
	/// We translate last (rightmost in matrix multiplication)
	/// This ensures that rotation and scaling happen around the object's origin
	transform = glm::translate(transform, this->position);

	/// Apply rotation
	/// We convert the quaternion to a rotation matrix
	/// Quaternions are used for rotation as they avoid gimbal lock and
	/// provide smooth interpolation
	transform = transform * glm::mat4_cast(this->rotation);

	/// Apply scale
	/// We scale first (leftmost in matrix multiplication)
	/// This ensures that scaling happens before rotation
	transform = glm::scale(transform, this->scale);

	return transform;
}

Transform Transform::fromMatrix(const glm::mat4& matrix) {
	/// Create a new transform to store the decomposed components
	Transform result;

	/// For decomposing the matrix, we need several components
	/// We use glm's built-in decompose function which provides:
	/// - Scale as a vec3
	/// - Rotation as a quaternion
	/// - Translation as a vec3
	/// - Skew as a vec3 (which we ignore)
	/// - Perspective as a vec4 (which we ignore)
	glm::vec3 skew;
	glm::vec4 perspective;

	/// Attempt to decompose the matrix
	/// This can fail if the matrix is not a valid transformation matrix
	/// (e.g., if it contains shear or other non-uniform transformations)
	bool success = glm::decompose(
		matrix,
		result.scale,
		result.rotation,
		result.position,
		skew,
		perspective
	);

	if (!success) {
		/// If decomposition fails, we log a warning and return identity transform
		/// This is better than throwing an exception as a failed decomposition
		/// is usually not a critical error
		spdlog::warn("Failed to decompose transformation matrix");
		return Transform{};
	}

	/// Normalize the quaternion to ensure it represents a valid rotation
	/// This prevents numerical errors from accumulating over time
	result.rotation = glm::normalize(result.rotation);

	/// Log the decomposed transform for debugging
	spdlog::trace("Matrix decomposed - Position: ({}, {}, {}), Scale: ({}, {}, {})",
		result.position.x, result.position.y, result.position.z,
		result.scale.x, result.scale.y, result.scale.z);

	return result;
}

} /// namespace lillugsi::scene