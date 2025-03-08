#pragma once

#include "material.h"
#include "texture.h"
#include "vulkan/vulkanwrappers.h"
#include <glm/glm.hpp>

namespace lillugsi::rendering {

/// PBRMaterial implements a physically-based rendering material
/// We use the metallic-roughness workflow as it's widely adopted and
/// provides good artistic control while maintaining physical accuracy
class PBRMaterial : public Material {
protected:
	/// Define the texture type enumeration for configuring specific texture settings
	/// This allows us to target specific textures when setting properties like tiling
	enum class TextureType {
		Albedo,
		Normal,
		Roughness,
		Metallic,
		Occlusion,
		RoughnessMetallic,
		OcclusionRoughnessMetallic
	};

public:
	/// Default shader paths for PBR material
	/// These static constants define the standard PBR shader locations
	/// which can be overridden in the constructor if needed
	static constexpr const char* DefaultVertexShaderPath = "shaders/pbr.vert.spv";
	static constexpr const char* DefaultFragmentShaderPath = "shaders/pbr.frag.spv";

	/// Create a new PBR material
	/// @param device The logical device for creating GPU resources
	/// @param name Unique name for this material instance
	/// @param physicalDevice The logical device for findMemoryType
	/// @param vertexShaderPath Optional path to custom vertex shader
	/// @param fragmentShaderPath Optional path to custom fragment shader
	PBRMaterial(
		VkDevice device,
		const std::string& name,
		VkPhysicalDevice physicalDevice,
		const std::string& vertexShaderPath = DefaultVertexShaderPath,
		const std::string& fragmentShaderPath = DefaultFragmentShaderPath
	);
	~PBRMaterial() override;

	/// Get the shader paths for this PBR material
	/// @return Shader paths configuration for PBR pipeline creation
	[[nodiscard]] ShaderPaths getShaderPaths() const override;

	/// Set the base color of the material
	/// @param color RGB color with alpha
	void setBaseColor(const glm::vec4& color);

	/// Set the roughness value
	/// Higher values create a more diffuse appearance
	/// @param roughness Value between 0 (smooth) and 1 (rough)
	void setRoughness(float roughness);

	/// Set the metallic value
	/// Controls how metallic the surface appears
	/// @param metallic Value between 0 (dielectric) and 1 (metallic)
	void setMetallic(float metallic);

	/// Set the ambient occlusion value
	/// This approximates how much ambient light reaches the surface
	/// @param ambient Value between 0 (fully occluded) and 1 (unoccluded)
	void setAmbient(float ambient);

	/// Set the normal map strength
	/// Controls how much influence the normal map has on surface normals
	/// @param strength Value between 0 (no effect) and 1 (full effect)
	void setNormalStrength(float strength);

	/// Set the roughness map strength
	/// Controls blending between base roughness and texture values
	/// @param strength Value between 0 (use base value) and 1 (use texture value)
	void setRoughnessStrength(float strength);

	/// Set the metallic map strength
	/// Controls blending between base metallic and texture values
	/// @param strength Value between 0 (use base value) and 1 (use texture value)
	void setMetallicStrength(float strength);

	/// Set the occlusion map strength
	/// Controls how much influence the occlusion map has
	/// @param strength Value between 0 (no occlusion) and 1 (full occlusion effect)
	void setOcclusionStrength(float strength);

	/// Set the albedo texture for this material
	/// This texture provides the base color across the surface
	/// @param texture Shared pointer to the texture to use
	void setAlbedoTexture(std::shared_ptr<Texture> texture);

	/// Set the normal map texture
	/// This texture defines surface detail through normal perturbation
	/// Normal maps are expected to be in tangent space (RGB -> XYZ)
	/// @param texture Shared pointer to the normal map texture
	/// @param strength Optional strength factor (0-1)
	void setNormalMap(std::shared_ptr<Texture> texture, float strength = 1.0f);

	/// Set the roughness map texture
	/// Controls surface microfacet distribution - how rough/smooth the surface appears
	/// Typically stored in the R channel with white being rough, black being smooth
	/// @param texture Shared pointer to the roughness map texture
	/// @param strength Optional strength factor (0-1)
	void setRoughnessMap(std::shared_ptr<Texture> texture, float strength = 1.0f);

	/// Set the metallic map texture
	/// Controls which parts of the surface are metallic vs. dielectric
	/// Typically stored in the R channel with white being metallic, black being dielectric
	/// @param texture Shared pointer to the metallic map texture
	/// @param strength Optional strength factor (0-1)
	void setMetallicMap(std::shared_ptr<Texture> texture, float strength = 1.0f);

	/// Set the ambient occlusion map texture
	/// Defines how much ambient light reaches different parts of the surface
	/// Typically stored in the R channel with white being unoccluded, black being occluded
	/// @param texture Shared pointer to the occlusion map texture
	/// @param strength Optional strength factor (0-1)
	void setOcclusionMap(std::shared_ptr<Texture> texture, float strength = 1.0f);

	/// Set a combined roughness-metallic map
	/// Many PBR workflows store roughness in G channel and metallic in B channel
	/// This saves texture memory and reduces sampler usage
	/// @param texture Shared pointer to the combined roughness-metallic map
	/// @param roughChannel Channel to sample roughness from (default G)
	/// @param metalChannel Channel to sample metallic from (default B)
	/// @param roughStrength Optional roughness strength factor (0-1)
	/// @param metalStrength Optional metallic strength factor (0-1)
	void setRoughnessMetallicMap(
		std::shared_ptr<Texture> texture,
		TextureChannel roughChannel = TextureChannel::G,
		TextureChannel metalChannel = TextureChannel::B,
		float roughStrength = 1.0f,
		float metalStrength = 1.0f
	);

	/// Set a combined occlusion-roughness-metallic map (ORM)
	/// Industry standard format packing all three parameters in RGB channels
	/// Typically: R = occlusion, G = roughness, B = metallic
	/// @param texture Shared pointer to the combined ORM map
	/// @param occlusionChannel Channel for occlusion (default R)
	/// @param roughnessChannel Channel for roughness (default G)
	/// @param metallicChannel Channel for metallic (default B)
	/// @param occlusionStrength Optional occlusion strength factor (0-1)
	/// @param roughnessStrength Optional roughness strength factor (0-1)
	/// @param metallicStrength Optional metallic strength factor (0-1)
	void setOcclusionRoughnessMetallicMap(
		std::shared_ptr<Texture> texture,
		TextureChannel occlusionChannel = TextureChannel::R,
		TextureChannel roughnessChannel = TextureChannel::G,
		TextureChannel metallicChannel = TextureChannel::B,
		float occlusionStrength = 1.0f,
		float roughnessStrength = 1.0f,
		float metallicStrength = 1.0f
	);

	/// Set texture coordinates tiling for all textures
	/// This affects how textures repeat across the surface
	/// @param uTiling Horizontal tiling factor (1.0 = no tiling)
	/// @param vTiling Vertical tiling factor (1.0 = no tiling)
	void setTextureTiling(float uTiling, float vTiling);

	/// Set texture coordinates tiling for a specific texture type
	/// This allows different tiling scales for each texture type
	/// @param textureType The type of texture to set tiling for
	/// @param uTiling Horizontal tiling factor
	/// @param vTiling Vertical tiling factor
	void setTextureTiling(TextureType textureType, float uTiling, float vTiling);

	/// Get the current albedo texture
	/// @return Shared pointer to the current albedo texture, or nullptr if none set
	[[nodiscard]] std::shared_ptr<Texture> getAlbedoTexture() const {
		return this->albedoTexture;
	}

	/// Get the current normal map texture
	/// @return Shared pointer to the normal map, or nullptr if none set
	[[nodiscard]] std::shared_ptr<Texture> getNormalMap() const {
		return this->normalMap;
	}

	/// Get the current roughness map texture
	/// @return Shared pointer to the roughness map, or nullptr if none set
	[[nodiscard]] std::shared_ptr<Texture> getRoughnessMap() const {
		return this->roughnessMap;
	}

	/// Get the current metallic map texture
	/// @return Shared pointer to the metallic map, or nullptr if none set
	[[nodiscard]] std::shared_ptr<Texture> getMetallicMap() const {
		return this->metallicMap;
	}

	/// Get the current occlusion map texture
	/// @return Shared pointer to the occlusion map, or nullptr if none set
	[[nodiscard]] std::shared_ptr<Texture> getOcclusionMap() const {
		return this->occlusionMap;
	}

	/// Get the normal map strength
	/// @return The current normal map strength (0-1)
	[[nodiscard]] float getNormalStrength() const {
		return this->properties.normalStrength;
	}

	/// Get the roughness map strength
	/// @return The current roughness map strength (0-1)
	[[nodiscard]] float getRoughnessStrength() const {
		return this->properties.roughnessStrength;
	}

	/// Get the metallic map strength
	/// @return The current metallic map strength (0-1)
	[[nodiscard]] float getMetallicStrength() const {
		return this->properties.metallicStrength;
	}

	/// Get the occlusion map strength
	/// @return The current occlusion map strength (0-1)
	[[nodiscard]] float getOcclusionStrength() const {
		return this->properties.occlusionStrength;
	}

	/// Bind this material's resources for rendering
	/// This method overrides the base class implementation to bind textures
	/// @param cmdBuffer The command buffer to record binding commands to
	/// @param pipelineLayout The pipeline layout for binding
	void bind(VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout) const override;

	/// Get the descriptor set layout for this material type
	/// Overridden to include texture samplers in the layout
	/// @return The descriptor set layout for this material
	[[nodiscard]] VkDescriptorSetLayout getDescriptorSetLayout() const override;

protected:
/// GPU-aligned material properties structure
/// We use this layout to match the shader's uniform buffer
///
/// IMPORTANT: This structure must match the layout expected by the shader
/// We use explicit alignment to ensure compatibility across hardware
/// std140 layout rules require specific alignment for different types:
/// - Scalars (float/int): 4 bytes
/// - vec2: 8 bytes
/// - vec3/vec4: 16 bytes
/// - nested structures and arrays have their own rules
struct Properties {
	/// Basic PBR properties
	alignas(16) glm::vec4 baseColor{1.0f}; /// RGB + alpha (16 bytes)
	alignas(4) float roughness{0.5f};      /// Default: medium roughness (4 bytes)
	alignas(4) float metallic{0.0f};       /// Default: dielectric (4 bytes)
	alignas(4) float ambient{1.0f};        /// Default: fully unoccluded (4 bytes)

	/// Texture flags (0=disabled, 1=enabled)
	alignas(4) float useAlbedoTexture{0.0f}; /// Whether to use albedo texture (4 bytes)
	alignas(4) float useNormalMap{0.0f};     /// Whether to use normal map (4 bytes)
	alignas(4) float useRoughnessMap{0.0f};  /// Whether to use roughness map (4 bytes)
	alignas(4) float useMetallicMap{0.0f};   /// Whether to use metallic map (4 bytes)
	alignas(4) float useOcclusionMap{0.0f};  /// Whether to use occlusion map (4 bytes)

	/// Texture influence strength factors
	alignas(4) float normalStrength{1.0f};     /// Normal map influence strength (4 bytes)
	alignas(4) float roughnessStrength{1.0f};  /// Roughness map influence strength (4 bytes)
	alignas(4) float metallicStrength{1.0f};   /// Metallic map influence strength (4 bytes)
	alignas(4) float occlusionStrength{1.0f};  /// Occlusion map influence strength (4 bytes)

	/// Texture coordinate tiling factors
	alignas(8) glm::vec2 albedoTiling{1.0f};     /// Albedo texture tiling (8 bytes)
	alignas(8) glm::vec2 normalTiling{1.0f};     /// Normal map tiling (8 bytes)
	alignas(8) glm::vec2 roughnessTiling{1.0f};  /// Roughness map tiling (8 bytes)
	alignas(8) glm::vec2 metallicTiling{1.0f};   /// Metallic map tiling (8 bytes)
	alignas(8) glm::vec2 occlusionTiling{1.0f};  /// Occlusion map tiling (8 bytes)

	/// Channel masks for texture sampling
	/// These define which channels to use from combined textures
	alignas(4) uint32_t roughnessChannel{1};  /// Default: G channel (R=0, G=1, B=2, A=3) (4 bytes)
	alignas(4) uint32_t metallicChannel{2};   /// Default: B channel (4 bytes)
	alignas(4) uint32_t occlusionChannel{0};  /// Default: R channel (4 bytes)

	/// Calculate total size for debugging
	/// This is useful for verifying alignment and buffer requirements
	static constexpr size_t computeSize() {
		return sizeof(glm::vec4) +       // baseColor
			   sizeof(float) * 12 +      // scalar properties and flags
			   sizeof(glm::vec2) * 5 +   // tiling factors
			   sizeof(uint32_t) * 3;     // channel masks
	}
};

	/// Create the descriptor set layout for PBR materials
	/// This defines what resources the material shader can access
	void createDescriptorSetLayout();

	/// Create and initialize the uniform buffer
	/// This holds the material properties on the GPU
	void createUniformBuffer();

	/// Create and initialize the descriptor set
	/// This allocates the descriptor set from our pool and
	/// updates it with the uniform buffer and texture information
	void createDescriptorSet();

	/// Update the uniform buffer with current properties
	/// Called whenever material properties change
	void updateUniformBuffer();

	/// Update descriptor set to reflect current textures
	/// Called when textures change or are assigned
	void updateTextureDescriptors();

	/// Convert a texture channel enum to a bit mask for the shader
	/// @param channel The texture channel to convert
	/// @return A bit mask for the specified channel
	[[nodiscard]] uint32_t channelToMask(TextureChannel channel) const;

	/// Create a descriptor pool for PBR material descriptors
	/// Overrides the base class implementation to provide PBR-specific configuration
	/// @return True if the pool was created successfully
	bool createDescriptorPool() override;
	void validateUniformBuffer() const;

	Properties properties;   /// CPU-side material properties

	/// Shader paths stored for pipeline creation
	std::string vertexShaderPath;
	std::string fragmentShaderPath;

	/// Texture resources
	std::shared_ptr<Texture> albedoTexture;    /// Base color texture
	std::shared_ptr<Texture> normalMap;        /// Normal map texture
	std::shared_ptr<Texture> roughnessMap;     /// Roughness map texture
	std::shared_ptr<Texture> metallicMap;      /// Metallic map texture
	std::shared_ptr<Texture> occlusionMap;     /// Ambient occlusion map texture
	std::shared_ptr<Texture> roughnessMetallicMap;  /// Combined roughness-metallic texture
	std::shared_ptr<Texture> ormMap;           /// Combined occlusion-roughness-metallic texture

	/// Texture configuration
	/// We track which maps are actually used to optimize descriptor updates
	/// and shader resource binding
	bool hasAlbedoTexture{false};
	bool hasNormalMap{false};
	bool hasRoughnessMap{false};
	bool hasMetallicMap{false};
	bool hasOcclusionMap{false};
	bool hasRoughnessMetallicMap{false};
	bool hasOrmMap{false};
};

} /// namespace lillugsi::rendering