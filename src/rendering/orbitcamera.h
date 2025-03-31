#pragma once

#include "camera.h"
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>

namespace lillugsi::rendering {

/// OrbitCamera class for object-centric camera movement
/// This camera orbits around a fixed target point, allowing for intuitive object inspection
/// Unlike the EditorCamera which moves freely through the scene, the OrbitCamera always
/// maintains focus on a specific point, making it ideal for modeling and object examination
class OrbitCamera : public Camera {
public:
	/// Constructor with default values positioned along negative Z-axis looking at origin
	/// @param targetPoint The point to orbit around - defaults to origin for common use case
	/// @param distance Initial distance from target - affects the perceived "zoom" level
	/// @param horizontalAngle Initial horizontal rotation in degrees - defaults to looking along -Z axis
	/// @param verticalAngle Initial vertical rotation in degrees - defaults to eye-level view
	OrbitCamera(
		const glm::vec3& targetPoint = glm::vec3(0.0f, 0.0f, 0.0f),
		float distance = 5.0f,
		float horizontalAngle = -90.0f,
		float verticalAngle = 0.0f
	);

	/// Handle input events for camera control
	/// @param window The SDL window for mouse capture operations
	/// @param event The SDL event to process
	void handleInput(SDL_Window* window, const SDL_Event& event);

	/// Update the camera's state based on time progression
	/// @param deltaTime The time elapsed since the last frame
	void update(float deltaTime) override;

	/// Get the view matrix for this camera
	/// The view matrix transforms from world space to camera space,
	/// oriented to always face the target point
	/// @return The view matrix
	[[nodiscard]] glm::mat4 getViewMatrix() const override;

	/// Get the projection matrix for this camera
	/// @param aspectRatio The current aspect ratio of the viewport
	/// @return The projection matrix
	[[nodiscard]] glm::mat4 getProjectionMatrix(float aspectRatio) const override;

	/// Set the target point to orbit around
	/// @param newTarget The new target point in world space
	void setTargetPoint(const glm::vec3& newTarget);

	/// Get the current target point
	/// @return The current target point in world space
	[[nodiscard]] glm::vec3 getTargetPoint() const;

	/// Set the orbit distance from the target
	/// @param newDistance The new distance from camera to target
	void setDistance(float newDistance);

	/// Get the current orbit distance
	/// @return The current distance from camera to target
	[[nodiscard]] float getDistance() const;

	/// Set the mouse sensitivity for orbit rotation
	/// @param sensitivity The new mouse sensitivity
	void setMouseSensitivity(float sensitivity);

	/// Set the zoom sensitivity for scroll wheel input
	/// @param sensitivity The new zoom sensitivity
	void setZoomSensitivity(float sensitivity);

private:
	/// Update the camera position based on current angles and distance
	/// This method recalculates the camera position while maintaining orientation toward the target
	void updateCameraPosition();

	/// Update the camera's quaternion orientation based on the current angles
	/// We convert from Euler-style angles to quaternion to avoid gimbal lock during rendering
	void updateOrientation();

	glm::vec3 targetPoint; /// The point to orbit around
	float distance; /// Distance from camera to target point

	/// Horizontal rotation angle in degrees (around Y-axis)
	/// This angle determines the left-right position of the camera in its orbit
	float horizontalAngle;

	/// Vertical rotation angle in degrees (around local X-axis)
	/// This angle determines the up-down position of the camera in its orbit
	/// Will be constrained to avoid flipping at the poles
	float verticalAngle;

	float mouseSensitivity; /// How quickly the camera rotates with mouse movement
	float zoomSensitivity; /// How quickly the camera zooms with scroll wheel

	/// Minimum allowed distance from target (prevents getting too close)
	/// This prevents clipping through the target or excessive perspective distortion
	float minDistance;

	/// Maximum allowed distance from target (prevents getting too far)
	/// This keeps the target from becoming too small to see effectively
	float maxDistance;

	/// Flag to track when orbit rotation is active
	/// We only orbit when this is true, typically when left mouse button is held
	bool isOrbiting;
};

} /// namespace namespace lillugsi::rendering