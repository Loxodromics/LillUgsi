#pragma once

#include "camera.h"
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>

/// Editor-style camera class
/// This camera allows for free movement in 3D space, controlled by mouse and keyboard input
/// It's suitable for 3D editing applications or first-person-style navigation
namespace lillugsi::rendering {
class EditorCamera : public Camera {
public:
	/// Constructor
	/// @param position Initial position of the camera
	/// @param yaw Initial yaw angle in degrees
	/// @param pitch Initial pitch angle in degrees
	EditorCamera(const glm::vec3& position = glm::vec3(0.0f, 0.0f, 0.0f), float yaw = -90.0f, float pitch = 0.0f);

	/// Handle input events for camera control
	/// This method should be called for each relevant SDL event
	/// @param event The SDL event to process
	void handleInput(SDL_Window* window, const SDL_Event& event);

	/// Update the camera's state
	/// This method should be called once per frame to update the camera's position and orientation
	/// @param deltaTime The time elapsed since the last frame
	void update(float deltaTime) override;

	/// Get the view matrix for this camera
	/// @return The view matrix that transforms world space to camera space
	glm::mat4 getViewMatrix() const override;

	/// Get the projection matrix for this camera
	/// @param aspectRatio The current aspect ratio of the viewport
	/// @return The projection matrix that transforms camera space to clip space
	glm::mat4 getProjectionMatrix(float aspectRatio) const override;

	/// Set the camera's movement speed
	/// @param speed The new movement speed
	void setMovementSpeed(float speed) { this->movementSpeed = speed; }

	/// Set the mouse sensitivity
	/// @param sensitivity The new mouse sensitivity
	void setMouseSensitivity(float sensitivity) { this->mouseSensitivity = sensitivity; }

private:
	/// Update the camera's orientation based on mouse movement
	/// @param xoffset The mouse x-axis offset since last update
	/// @param yoffset The mouse y-axis offset since last update
	void updateOrientation(float xoffset, float yoffset);

	/// Update the camera's vectors (front, up, right) based on the current orientation
	void updateCameraVectors();

	float yaw; /// Yaw angle in degrees
	float pitch; /// Pitch angle in degrees
	float movementSpeed; /// Camera movement speed
	float mouseSensitivity; /// Mouse sensitivity for orientation control

	bool isMouseLookActive; /// Flag to indicate if mouse look is currently active

	glm::vec3 velocity; /// Current velocity of the camera
};
}
