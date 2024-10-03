#include "editorcamera.h"
#include <spdlog/spdlog.h>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <SDL3/SDL_mouse.h>

namespace lillugsi::rendering {
EditorCamera::EditorCamera(const glm::vec3& position, float yaw, float pitch)
	: Camera()
	  , yaw(yaw)
	  , pitch(pitch)
	  , movementSpeed(5.0f)
	  , mouseSensitivity(0.1f)
	  , isMouseLookActive(false)
	  , velocity(0.0f) {
	this->setPosition(position);
	this->updateCameraVectors();
}

void EditorCamera::handleInput(SDL_Window* window, const SDL_Event& event) {
	switch (event.type) {
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
		if (event.button.button == SDL_BUTTON_RIGHT) {
			/// Activate mouse look when right mouse button is pressed
				/// This allows for intuitive camera rotation control
			this->isMouseLookActive = true;
			SDL_SetWindowRelativeMouseMode(window, true);
		}
		break;
	case SDL_EVENT_MOUSE_BUTTON_UP:
		if (event.button.button == SDL_BUTTON_RIGHT) {
			/// Deactivate mouse look when right mouse button is released
			this->isMouseLookActive = false;
			SDL_SetWindowRelativeMouseMode(window, false);
		}
		break;
	case SDL_EVENT_MOUSE_MOTION:
		if (this->isMouseLookActive) {
			/// Update camera orientation based on mouse movement
				/// We use relative mouse motion for smoother control
			this->updateOrientation(
				event.motion.xrel * this->mouseSensitivity,
				-event.motion.yrel * this->mouseSensitivity /// Inverted for natural feel
			);
		}
		break;
	case SDL_EVENT_KEY_DOWN:
		/// Update velocity based on key presses
			/// This allows for smooth camera movement
		if (event.key.key == SDLK_W)
			this->velocity.z = this->movementSpeed;
		if (event.key.key == SDLK_S)
			this->velocity.z = -this->movementSpeed;
		if (event.key.key == SDLK_A)
			this->velocity.x = -this->movementSpeed;
		if (event.key.key == SDLK_D)
			this->velocity.x = this->movementSpeed;
		if (event.key.key == SDLK_Q)
			this->velocity.y = -this->movementSpeed;
		if (event.key.key == SDLK_E)
			this->velocity.y = this->movementSpeed;
		break;
	case SDL_EVENT_KEY_UP:
		/// Reset velocity when keys are released
			/// This ensures the camera stops moving when the user releases the key
		if (event.key.key == SDLK_W || event.key.key == SDLK_S)
			this->velocity.z = 0.0f;
		if (event.key.key == SDLK_A || event.key.key == SDLK_D)
			this->velocity.x = 0.0f;
		if (event.key.key == SDLK_Q || event.key.key == SDLK_D)
			this->velocity.y = 0.0f;
		break;
	}
}

void EditorCamera::update(float deltaTime) {
	/// Update position based on velocity
	/// This creates smooth camera movement over time
	this->position += this->getFront() * this->velocity.z * deltaTime;
	this->position += this->getRight() * this->velocity.x * deltaTime;
	this->position += this->getUp() * this->velocity.y * deltaTime;

	/// Update camera vectors
	this->updateCameraVectors();
}

glm::mat4 EditorCamera::getViewMatrix() const {
	/// Use glm::lookAt to create the view matrix
	/// This matrix transforms world space to camera space
	return glm::lookAt(this->position, this->position + this->getFront(), this->getUp());
}

glm::mat4 EditorCamera::getProjectionMatrix(float aspectRatio) const {
	/// Use glm::perspective to create the projection matrix
	/// This matrix transforms camera space to clip space
	return glm::perspective(glm::radians(this->fov), aspectRatio, this->nearPlane, this->farPlane);
}

void EditorCamera::updateOrientation(float xoffset, float yoffset) {
	/// Update yaw and pitch based on mouse movement
	this->yaw += xoffset;
	this->pitch += yoffset;

	/// Constrain pitch to avoid flipping
	/// This prevents the camera from flipping upside down
	if (this->pitch > 89.0f)
		this->pitch = 89.0f;
	if (this->pitch < -89.0f)
		this->pitch = -89.0f;

	/// Update the camera vectors based on the new orientation
	this->updateCameraVectors();

	/// Debug output for camera position and orientation
	spdlog::trace("Camera Position: {}", glm::to_string(this->position));
	spdlog::trace("Camera Yaw: {}, Pitch: {}", this->yaw, this->pitch);
	spdlog::trace("Camera Front Vector: {}", glm::to_string(this->getFront()));
	spdlog::trace("Camera Up Vector: {}", glm::to_string(this->getUp()));
	spdlog::trace("Camera Right Vector: {}", glm::to_string(this->getRight()));

}

void EditorCamera::updateCameraVectors() {
	/// Calculate the new front vector
	glm::vec3 front;
	front.x = cos(glm::radians(this->yaw)) * cos(glm::radians(this->pitch));
	front.y = sin(glm::radians(this->pitch));
	front.z = sin(glm::radians(this->yaw)) * cos(glm::radians(this->pitch));
	front = glm::normalize(front);

	/// Update the camera's orientation quaternion
	/// We use a quaternion to avoid gimbal lock and enable smooth interpolation
	this->orientation = glm::quatLookAt(front, glm::vec3(0.0f, 1.0f, 0.0f));
}
}
