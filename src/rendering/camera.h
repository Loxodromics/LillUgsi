#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

/// Base class for all camera implementations
/// This class provides the common interface and properties for different camera types
/// It uses quaternions for rotation to avoid gimbal lock and enable smooth interpolation
namespace lillugsi::rendering {
class Camera {
public:
	/// Virtual destructor to ensure proper cleanup of derived classes
	virtual ~Camera() = default;

	/// Get the view matrix for this camera
	/// The view matrix transforms world space to camera space
	virtual glm::mat4 getViewMatrix() const = 0;

	/// Get the projection matrix for this camera
	/// The projection matrix transforms camera space to clip space
	/// @param aspectRatio The current aspect ratio of the viewport
	virtual glm::mat4 getProjectionMatrix(float aspectRatio) const = 0;

	/// Update the camera's state
	/// This method should be called once per frame to update the camera's internal state
	/// @param deltaTime The time elapsed since the last frame
	virtual void update(float deltaTime) = 0;

	/// Set the camera's position in world space
	virtual void setPosition(const glm::vec3& newPosition) { this->position = newPosition; }

	/// Get the camera's position in world space
	virtual glm::vec3 getPosition() const { return this->position; }

	/// Set the camera's orientation using a quaternion
	virtual void setOrientation(const glm::quat& newOrientation) { this->orientation = newOrientation; }

	/// Get the camera's orientation as a quaternion
	virtual glm::quat getOrientation() const { return this->orientation; }

	/// Set the camera's field of view in degrees
	virtual void setFov(float newFov) { this->fov = newFov; }

	/// Get the camera's field of view in degrees
	virtual float getFov() const { return this->fov; }

	/// Set the camera's near clipping plane distance
	virtual void setNearPlane(float newNearPlane) { this->nearPlane = newNearPlane; }

	/// Get the camera's near clipping plane distance
	virtual float getNearPlane() const { return this->nearPlane; }

	/// Set the camera's far clipping plane distance
	virtual void setFarPlane(float newFarPlane) { this->farPlane = newFarPlane; }

	/// Get the camera's far clipping plane distance
	virtual float getFarPlane() const { return this->farPlane; }

protected:
	/// The camera's position in world space
	glm::vec3 position{0.0f, 0.0f, 0.0f};

	/// The camera's orientation as a quaternion
	/// Using a quaternion instead of Euler angles avoids gimbal lock
	glm::quat orientation{1.0f, 0.0f, 0.0f, 0.0f};  // Identity quaternion

	/// The camera's field of view in degrees
	float fov = 45.0f;

	/// The distance to the near clipping plane
	float nearPlane = 0.1f;

	/// The distance to the far clipping plane
	float farPlane = 100.0f;

	/// Get the camera's front vector (the direction it's looking)
	glm::vec3 getFront() const { return this->orientation * glm::vec3(0.0f, 0.0f, -1.0f); }

	/// Get the camera's up vector
	glm::vec3 getUp() const { return this->orientation * glm::vec3(0.0f, 1.0f, 0.0f); }

	/// Get the camera's right vector
	glm::vec3 getRight() const { return this->orientation * glm::vec3(1.0f, 0.0f, 0.0f); }
};
}
