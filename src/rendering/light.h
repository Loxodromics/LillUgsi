#pragma once

#include <glm/glm.hpp>
#include <cstdint>

namespace lillugsi::rendering {

/// LightData represents the GPU-side data structure for lights
/// We align this structure to meet Vulkan's requirements for uniform buffers
/// The structure is designed to be efficiently packed and aligned for GPU access
struct alignas(16) LightData {
	/// Direction vector for the light
	/// We use vec4 instead of vec3 for alignment purposes
	/// The w component is unused but provides padding
	glm::vec4 direction{0.0f, -1.0f, 0.0f, 0.0f};

	/// Light color and intensity
	/// RGB components represent color, w component stores intensity
	/// This combines color and intensity in one vector to reduce uniform buffer size
	/// All lights with colorAndIntensity > 0 will be added to the light calculation
	glm::vec4 colorAndIntensity{0.0f};

	/// Ambient contribution of the light
	/// We separate ambient from main color to allow for different ambient colors
	/// The w component is unused but maintains alignment
	glm::vec4 ambient{0.1f, 0.1f, 0.1f, 0.0f};
};

/// Base class for all light types
/// This provides common functionality and interface for different light types
/// We use a class hierarchy to support different light types while maintaining
/// a consistent interface for the renderer
class Light {
public:
	/// Virtual destructor ensures proper cleanup of derived classes
	virtual ~Light() = default;

	/// Get the light data formatted for GPU usage
	/// This method standardizes how light data is passed to shaders
	/// @return The GPU-compatible light data structure
	[[nodiscard]] virtual LightData getLightData() const = 0;

	/// Set the light's color
	/// @param color The RGB color of the light
	virtual void setColor(const glm::vec3& color) = 0;

	/// Get the light's color
	/// @return The current RGB color of the light
	[[nodiscard]] virtual glm::vec3 getColor() const = 0;

	/// Set the light's intensity
	/// @param intensity The scalar intensity of the light
	virtual void setIntensity(float intensity) = 0;

	/// Get the light's intensity
	/// @return The current intensity value
	[[nodiscard]] virtual float getIntensity() const = 0;

	/// Set the light's ambient color
	/// @param ambient The RGB ambient color contribution
	virtual void setAmbient(const glm::vec3& ambient) = 0;

	/// Get the light's ambient color
	/// @return The current ambient color
	[[nodiscard]] virtual glm::vec3 getAmbient() const = 0;
};

/// DirectionalLight represents a light source with parallel rays
/// This type of light is useful for simulating distant light sources like the sun
/// All rays from a directional light are parallel, making it efficient to compute
class DirectionalLight : public Light {
public:
	/// Constructor initializes the light with default values
	/// We use sensible defaults that can be adjusted later
	explicit DirectionalLight();

	/// Constructor with initial direction
	/// This allows setting the light direction during creation
	/// @param direction The initial direction vector (will be normalized)
	explicit DirectionalLight(const glm::vec3& direction);

	/// Set the light's direction
	/// @param direction The direction vector (will be normalized)
	void setDirection(const glm::vec3& direction);

	/// Get the light's direction
	/// @return The normalized direction vector
	[[nodiscard]] glm::vec3 getDirection() const;

	/// Implementation of base class virtual methods
	[[nodiscard]] LightData getLightData() const override;
	void setColor(const glm::vec3& color) override;
	[[nodiscard]] glm::vec3 getColor() const override;
	void setIntensity(float intensity) override;
	[[nodiscard]] float getIntensity() const override;
	void setAmbient(const glm::vec3& ambient) override;
	[[nodiscard]] glm::vec3 getAmbient() const override;

private:
	glm::vec3 direction{0.0f, -1.0f, 0.0f};  /// Light direction (normalized)
	glm::vec3 color{1.0f};                   /// Light color (RGB)
	float intensity{1.0f};                    /// Light intensity
	glm::vec3 ambient{0.1f};                 /// Ambient light color
};

} /// namespace lillugsi::rendering