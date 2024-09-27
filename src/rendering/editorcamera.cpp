#include "editorcamera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <SDL3/SDL_mouse.h>

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
				this->isMouseLookActive = true;
				SDL_SetWindowRelativeMouseMode(window, true);
			}
			break;
		case SDL_EVENT_MOUSE_BUTTON_UP:
			if (event.button.button == SDL_BUTTON_RIGHT) {
				this->isMouseLookActive = false;
				SDL_SetWindowRelativeMouseMode(window, false);
			}
			break;
		case SDL_EVENT_MOUSE_MOTION:
			if (this->isMouseLookActive) {
				this->updateOrientation(
					event.motion.xrel * this->mouseSensitivity,
					-event.motion.yrel * this->mouseSensitivity /// Inverted for natural feel
				);
			}
			break;
		case SDL_EVENT_KEY_DOWN:
			/// Update velocity based on key presses
			if (event.key.key == SDLK_W)
				this->velocity.z = -this->movementSpeed;
			if (event.key.key == SDLK_S)
				this->velocity.z = this->movementSpeed;
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
	this->position += this->getFront() * this->velocity.z * deltaTime;
	this->position += this->getRight() * this->velocity.x * deltaTime;
	this->position += this->getUp() * this->velocity.y * deltaTime;

	/// Update camera vectors
	this->updateCameraVectors();
}

glm::mat4 EditorCamera::getViewMatrix() const {
	return glm::lookAt(this->position, this->position + this->getFront(), this->getUp());
}

glm::mat4 EditorCamera::getProjectionMatrix(float aspectRatio) const {
	return glm::perspective(glm::radians(this->fov), aspectRatio, this->nearPlane, this->farPlane);
}

void EditorCamera::updateOrientation(float xoffset, float yoffset) {
	this->yaw += xoffset;
	this->pitch += yoffset;

	/// Constrain pitch to avoid flipping
	if (this->pitch > 89.0f)
		this->pitch = 89.0f;
	if (this->pitch < -89.0f)
		this->pitch = -89.0f;

	this->updateCameraVectors();
}

void EditorCamera::updateCameraVectors() {
	/// Calculate the new front vector
	glm::vec3 front;
	front.x = cos(glm::radians(this->yaw)) * cos(glm::radians(this->pitch));
	front.y = sin(glm::radians(this->pitch));
	front.z = sin(glm::radians(this->yaw)) * cos(glm::radians(this->pitch));
	front = glm::normalize(front);

	/// Update the camera's orientation quaternion
	this->orientation = glm::quatLookAt(front, glm::vec3(0.0f, 1.0f, 0.0f));
}