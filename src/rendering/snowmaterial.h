#pragma once

#include "material.h"
#include "texture.h"
#include "vulkan/vulkanwrappers.h"
#include <glm/glm.hpp>

namespace lillugsi::rendering {

/// SnowMaterial provides a specialized material for realistic snow rendering
/// Phase 1 implementation includes:
/// - View-dependent albedo darkening for realistic brightness
/// - Fresnel rim lighting for edge highlighting
/// - Sparkle/glitter effects for snow crystals
/// Phase 2 adds:
/// - Subsurface scattering via pre-integrated LUT for soft shadow edges
class SnowMaterial : public Material {
public:
	/// Create a snow material with custom configuration
	/// @param device The logical device for creating GPU resources
	/// @param name Unique identifier for this material instance
	/// @param physicalDevice The physical device for memory allocation
	SnowMaterial(
		VkDevice device,
		const std::string& name,
		VkPhysicalDevice physicalDevice
	);
	~SnowMaterial() override;

	[[nodiscard]] ShaderPaths getShaderPaths() const override;

	/// View-dependent albedo darkening
	void setBaseAlbedo(const glm::vec4& albedo);
	void setDarkeningAmount(float amount);  /// 0.7-0.9 range
	void setDarkeningPower(float power);    /// 2-3 range

	/// Fresnel rim lighting
	void setFresnelPower(float power);      /// 3-5 range
	void setRimIntensity(float intensity);  /// 0.1-0.3 range
	void setSkyColor(const glm::vec3& color);

	/// Sparkle effect
	void setSparkleScale(float scale);      /// 50-200 range
	void setSparkleThreshold(float threshold); /// 0.98-0.995 range
	void setSparkleIntensity(float intensity); /// 5-20 HDR range

	/// Subsurface scattering (Phase 2)
	void setScatterColor(const glm::vec3& color);  /// Warm tint for depth scattering
	void setCurvatureScale(float scale);           /// Artistic control over SSS response (0.5-2.0)
	void setSSSLUT(std::shared_ptr<Texture> texture); /// Set the pre-integrated SSS lookup texture

	/// Getters for all properties
	[[nodiscard]] glm::vec4 getBaseAlbedo() const { return this->properties.baseAlbedo; }
	[[nodiscard]] float getDarkeningAmount() const { return this->properties.darkeningAmount; }
	[[nodiscard]] float getDarkeningPower() const { return this->properties.darkeningPower; }
	[[nodiscard]] float getFresnelPower() const { return this->properties.fresnelPower; }
	[[nodiscard]] float getRimIntensity() const { return this->properties.rimIntensity; }
	[[nodiscard]] glm::vec3 getSkyColor() const { return this->properties.skyColor; }
	[[nodiscard]] float getSparkleScale() const { return this->properties.sparkleScale; }
	[[nodiscard]] float getSparkleThreshold() const { return this->properties.sparkleThreshold; }
	[[nodiscard]] float getSparkleIntensity() const { return this->properties.sparkleIntensity; }
	[[nodiscard]] glm::vec3 getScatterColor() const { return this->properties.scatterColor; }
	[[nodiscard]] float getCurvatureScale() const { return this->properties.curvatureScale; }

public:
	/// Bind this material's resources for rendering
	/// Overridden to bind SSS LUT texture if present
	void bind(VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout) const override;

private:
	/// Create the descriptor layout for our uniform buffer and SSS LUT
	void createDescriptorSetLayout();

	/// Create the descriptor pool for uniform buffer and SSS sampler
	/// Overridden to include space for the SSS LUT sampler
	bool createDescriptorPool() override;

	/// Create and initialize the uniform buffer
	void createUniformBuffer();

	/// Create descriptor pool and set
	void createDescriptorSet();

	/// Update the uniform buffer with current properties
	void updateUniformBuffer();

	/// Update texture descriptors when SSS LUT changes
	void updateTextureDescriptors();

	/// GPU-aligned properties (must match shader layout)
	/// This structure matches the layout expected by our shaders
	struct Properties {
		alignas(16) glm::vec4 baseAlbedo{0.95f, 0.95f, 0.97f, 1.0f};

		/// View darkening
		alignas(4) float darkeningAmount{0.85f};
		alignas(4) float darkeningPower{2.5f};

		/// Rim lighting
		alignas(4) float fresnelPower{4.0f};
		alignas(4) float rimIntensity{0.2f};
		alignas(16) glm::vec3 skyColor{0.6f, 0.7f, 1.0f};
		alignas(4) float _pad1;

		/// Sparkle
		alignas(4) float sparkleScale{100.0f};
		alignas(4) float sparkleThreshold{0.985f};
		alignas(4) float sparkleIntensity{10.0f};
		alignas(4) float _pad2;

		/// Subsurface scattering (Phase 2)
		alignas(16) glm::vec3 scatterColor{1.0f, 0.98f, 0.95f};  /// Slight warm tint
		alignas(4) float curvatureScale{1.0f};
	};

	Properties properties;

	/// Texture resources (Phase 2)
	std::shared_ptr<Texture> sssLUT;  /// Pre-integrated subsurface scattering lookup texture

	/// Shader paths stored for pipeline creation
	static constexpr const char* vertexShaderPath = "shaders/snow.vert.spv";
	static constexpr const char* fragmentShaderPath = "shaders/snow.frag.spv";
};

} /// namespace lillugsi::rendering
