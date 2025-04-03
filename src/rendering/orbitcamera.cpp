#include "orbitcamera.h"

#include <spdlog/spdlog.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm> // For std::clamp

namespace lillugsi::rendering {

OrbitCamera::OrbitCamera(
	const glm::vec3 &targetPoint, float distance, float horizontalAngle, float verticalAngle)
	: Camera()
	, targetPoint(targetPoint)
	, distance(distance)
	, horizontalAngle(horizontalAngle)
	, verticalAngle(verticalAngle)
	, mouseSensitivity(0.25f)
	, zoomSensitivity(0.15f)
	, minDistance(0.5f)
	, maxDistance(100.0f)
	, isOrbiting(false) {
	/// Initialize camera with sensible defaults for orbit viewing
	/// These FOV and plane settings provide a balanced perspective for object inspection
	this->setFov(45.0f);
	this->setNearPlane(0.1f);
	this->setFarPlane(1000.0f);

	/// Constrain the initial vertical angle to prevent issues at poles
	/// We clamp between -89 and 89 degrees to avoid numerical instability at ±90 degrees
	this->verticalAngle = std::clamp(verticalAngle, -89.0f, 89.0f);

	/// Calculate the initial position and orientation
	/// This ensures the camera starts correctly positioned around the target
	this->updateCameraPosition();
}

void OrbitCamera::handleInput(SDL_Window *window, const SDL_Event &event) {
	spdlog::trace("SDL Event Type: {}", static_cast<int>(event.type));
	switch (event.type) {
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
		if (event.button.button == SDL_BUTTON_LEFT) {
			/// Activate orbiting when left mouse button is pressed
			/// This provides an intuitive way for users to indicate when they want to orbit
			/// rather than accidentally moving the camera with every mouse movement
			this->isOrbiting = true;

			/// Set relative mouse mode to capture the cursor
			/// This prevents the cursor from leaving the window and provides continuous input
			SDL_SetWindowRelativeMouseMode(window, true);
		}
		break;

	case SDL_EVENT_MOUSE_BUTTON_UP:
		if (event.button.button == SDL_BUTTON_LEFT) {
			/// Deactivate orbiting when left mouse button is released
			/// This allows users to position their cursor without affecting the camera
			this->isOrbiting = false;

			/// Release the cursor so it can move normally again
			SDL_SetWindowRelativeMouseMode(window, false);
		}
		break;

	case SDL_EVENT_MOUSE_MOTION:
		if (this->isOrbiting) {
			/// Process mouse movement for camera orbiting
			/// We only orbit when the user is holding the left mouse button

			/// Convert mouse movement to angle changes
			/// Horizontal movement (X) changes the horizontal angle
			/// Vertical movement (Y) changes the vertical angle
			float horizontalDelta = -event.motion.xrel * this->mouseSensitivity;
			float verticalDelta = -event.motion.yrel * this->mouseSensitivity;

			/// Update the orbit angles
			/// We negate the deltas because moving the mouse right should rotate right
			/// and moving the mouse up should rotate up
			this->horizontalAngle += horizontalDelta;
			this->verticalAngle += verticalDelta;

			/// Constrain vertical angle to prevent flipping at the poles
			/// This keeps the angle within ±89 degrees to avoid numeric instability
			/// and prevents the camera from flipping upside down
			this->verticalAngle = std::clamp(this->verticalAngle, -89.0f, 89.0f);

			/// Normalize horizontal angle to keep it within [0, 360) range
			/// This prevents the angle from growing infinitely as the user orbits multiple times
			this->horizontalAngle = std::fmod(this->horizontalAngle, 360.0f);
			if (this->horizontalAngle < 0.0f) {
				this->horizontalAngle += 360.0f;
			}

			/// Update camera position and orientation based on new angles
			this->updateCameraPosition();
		}
		break;

	case SDL_EVENT_MOUSE_WHEEL: {
			/// Process mouse wheel for zooming in and out
			/// This provides an intuitive way to change the distance from the target

			/// Convert wheel movement to distance change
			/// Scrolling up (positive Y) zooms in (decreases distance)
			/// Scrolling down (negative Y) zooms out (increases distance)
			float zoomAmount = -event.wheel.y * this->zoomSensitivity * this->distance;

			/// Apply the zoom by changing the distance
			/// We multiply by current distance to make zooming proportional to how far
			/// away we are - this gives more precise control when close to the object
			this->setDistance(this->distance + zoomAmount);
		}
		break;

	case SDL_EVENT_KEY_DOWN:
		/// Handle keyboard inputs for zooming when mouse wheel is unavailable
			/// This provides an alternative way to adjust the camera distance from target

			if (event.key.key == SDLK_PLUS || event.key.key == SDLK_KP_PLUS || event.key.key == SDLK_EQUALS) {
				/// Zoom in when + key is pressed
				/// Note: SDLK_EQUALS is included because on many keyboards, + is Shift+Equals
				float zoomAmount = -this->zoomSensitivity * this->distance;
				this->setDistance(this->distance + zoomAmount);
			}
			else if (event.key.key == SDLK_MINUS || event.key.key == SDLK_KP_MINUS) {
				/// Zoom out when - key is pressed
				float zoomAmount = this->zoomSensitivity * this->distance;
				this->setDistance(this->distance + zoomAmount);
			}
		break;

	default:
		break;
	}
}

void OrbitCamera::update(float deltaTime) {
	/// Handle any time-based updates for the orbit camera
	/// This is a minimal implementation for now - will be expanded
	/// in future iterations to handle momentum and smooth transitions

	/// Currently, all our updates are immediate based on input events
	/// No continuous updates are needed in this basic implementation
}

glm::mat4 OrbitCamera::getViewMatrix() const {
	/// Create a view matrix that properly orients the camera to look at the target
	/// This is the core function that ensures our camera always faces the orbit target

	/// We use a look-at matrix for maximum stability and clarity
	/// This approach directly ensures the camera is pointed at the target without
	/// the need for manual vector calculations or quaternion-to-matrix conversions
	return glm::lookAt(
		this->getPosition(), // Eye position (where the camera is)
		this->targetPoint,   // Center position (what we're looking at)
		this->getUp()        // Up vector (orientation of the camera)
	);

	/// Note: We rely on the Camera base class's getUp() method which already
	/// calculates the up vector from our quaternion orientation. This ensures
	/// consistent behavior with the rest of the camera system.
}

glm::mat4 OrbitCamera::getProjectionMatrix(float aspectRatio) const {
	/// Create a projection matrix that's consistent with the base camera class
	/// While we could customize the projection for orbiting cameras (e.g., for
	/// orthographic views), we maintain consistency with the existing system

	/// For perspective projection, we use the standard approach with the camera's FOV
	return glm::perspective(
		glm::radians(this->getFov()), /// Vertical field of view in radians
		aspectRatio,
		this->getFarPlane(), /// Far clipping plane and
		this->getNearPlane() /// Near clipping plane are revesed since we are using Reverse-Z
	);
}

void OrbitCamera::setTargetPoint(const glm::vec3& newTarget) {
	/// Store the new target point
	this->targetPoint = newTarget;

	/// Update camera position around the new target
	/// We need to recalculate position after changing the pivot point
	this->updateCameraPosition();
}

glm::vec3 OrbitCamera::getTargetPoint() const {
	return this->targetPoint;
}

void OrbitCamera::setDistance(float newDistance) {
	/// Clamp the distance between allowed min and max values
	/// This prevents getting too close (causing clipping) or too far (making target too small)
	this->distance = std::clamp(newDistance, this->minDistance, this->maxDistance);

	/// Update camera position to reflect the new distance
	this->updateCameraPosition();
}

float OrbitCamera::getDistance() const {
	return this->distance;
}

void OrbitCamera::setMouseSensitivity(float sensitivity) {
	/// Negative sensitivity would cause inverted controls, which might be confusing
	/// We ensure sensitivity is positive for intuitive control
	this->mouseSensitivity = std::max(0.01f, sensitivity);
}

void OrbitCamera::setZoomSensitivity(float sensitivity) {
	/// Ensure zoom sensitivity is positive for intuitive zooming behavior
	this->zoomSensitivity = std::max(0.01f, sensitivity);
}

void OrbitCamera::updateCameraPosition() {
	/// First update the orientation quaternion from the angles
	this->updateOrientation();

	/// Calculate the direction vector directly from the quaternion
	/// The -Z axis represents "forward" in OpenGL convention
	const glm::vec3 directionFromTarget = -this->getOrientation() * glm::vec3(0.0f, 0.0f, 1.0f);

	/// Position the camera at the correct distance from the target
	this->setPosition(this->targetPoint - directionFromTarget * this->distance);
}

void OrbitCamera::updateOrientation() {
	/// Create a quaternion from the horizontal and vertical angles
	/// This approach provides smooth rotation without gimbal lock issues

	/// Start with the base rotation (looking along -Z axis)
	/// We add 180 degrees to the horizontal angle to convert from our angle system
	/// (where -90 is looking along -Z) to the rotation system (where 0 is looking along +Z)
	float adjustedHorizontalAngle = this->horizontalAngle + 180.0f;

	/// Create a quaternion for rotation around the Y-axis (horizontal rotation)
	glm::quat horizontalQuat = glm::angleAxis(
		glm::radians(adjustedHorizontalAngle),
		glm::vec3(0.0f, 1.0f, 0.0f)
	);

	/// Create a quaternion for rotation around the local X-axis (vertical rotation)
	/// We negate the vertical angle to match expected behavior (up = negative angle)
	glm::quat verticalQuat = glm::angleAxis(
		glm::radians(-this->verticalAngle),
		glm::vec3(1.0f, 0.0f, 0.0f)
	);

	/// Combine the rotations, applying horizontal rotation first, then vertical
	/// This order ensures the vertical rotation happens around the local x-axis
	this->setOrientation(horizontalQuat * verticalQuat);
}

}