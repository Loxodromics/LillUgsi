#include "camera.h"

#include <glm/gtc/matrix_transform.hpp>

/// The Camera base class doesn't require any implementation in the .cpp file
/// as all its methods are either pure virtual or trivial.
/// The actual implementation will be in the derived classes.

/// However, we could add some utility functions here if needed.
/// For example, a function to create a look-at matrix:

glm::mat4 createLookAtMatrix(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up) {
	return glm::lookAt(position, target, up);
}

/// Or a function to create a perspective projection matrix:

glm::mat4 createPerspectiveMatrix(float fov, float aspectRatio, float nearPlane, float farPlane) {
	return glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
}
