#include "light.h"
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

namespace lillugsi::rendering {

DirectionalLight::DirectionalLight() {
	/// Normalize the default direction vector
	/// This ensures we start with a valid direction
	this->direction = glm::normalize(this->direction);
	spdlog::debug("Created directional light with default parameters");
}

DirectionalLight::DirectionalLight(const glm::vec3& direction)
	: color(1.0f)
	, intensity(1.0f)
	, ambient(0.1f) {
	/// Use the existing setDirection method to ensure proper normalization
	/// and validation of the input direction
	this->setDirection(direction);
	spdlog::debug("Created directional light with direction ({}, {}, {})",
		this->direction.x, this->direction.y, this->direction.z);
}

void DirectionalLight::setDirection(const glm::vec3& direction) {
	/// Validate input to prevent zero-length direction vector
	/// A zero vector would result in undefined lighting behavior
	if (glm::length(direction) < 1e-6f) {
		spdlog::warn("Attempted to set zero direction vector for directional light");
		return;
	}

	/// Store normalized direction
	/// Normalization ensures consistent lighting calculations
	this->direction = glm::normalize(direction);
	spdlog::trace("Set directional light direction to ({}, {}, {})",
		this->direction.x, this->direction.y, this->direction.z);
}

glm::vec3 DirectionalLight::getDirection() const {
	return this->direction;
}

LightData DirectionalLight::getLightData() const {
	LightData data;
	
	/// Pack direction into vec4 for GPU
	/// We use vec4 to maintain alignment requirements
	data.direction = glm::vec4(this->direction, 0.0f);
	
	/// Combine color and intensity into vec4
	/// This reduces the number of uniforms needed in the shader
	data.colorAndIntensity = glm::vec4(this->color * this->intensity, 1.0f);
	
	/// Pack ambient color into vec4
	data.ambient = glm::vec4(this->ambient, 0.0f);
	
	return data;
}

void DirectionalLight::setColor(const glm::vec3& color) {
	/// Store color as-is
	/// No normalization needed as colors can have any positive value
	this->color = color;
	spdlog::trace("Set directional light color to ({}, {}, {})",
		color.r, color.g, color.b);
}

glm::vec3 DirectionalLight::getColor() const {
	return this->color;
}

void DirectionalLight::setIntensity(float intensity) {
	/// Clamp intensity to positive values
	/// Negative light intensities don't make physical sense
	this->intensity = std::max(0.0f, intensity);
	spdlog::trace("Set directional light intensity to {}", this->intensity);
}

float DirectionalLight::getIntensity() const {
	return this->intensity;
}

void DirectionalLight::setAmbient(const glm::vec3& ambient) {
	/// Store ambient color as-is
	/// Like main color, ambient can have any positive value
	this->ambient = ambient;
	spdlog::trace("Set directional light ambient to ({}, {}, {})",
		ambient.r, ambient.g, ambient.b);
}

glm::vec3 DirectionalLight::getAmbient() const {
	return this->ambient;
}

} /// namespace lillugsi::rendering