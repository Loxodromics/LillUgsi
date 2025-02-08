#pragma once

#include "material.h"
#include "vulkan/vulkanwrappers.h"
#include <glm/glm.hpp>

namespace lillugsi::rendering {

/// TerrainMaterial implements height-based biome visualization for planetary surfaces
/// We extend from the base Material class to leverage the existing material system.
/// This first version focuses on interpreting vertex height data (stored in vertex colors)
/// to create basic biome visualization as a foundation for more complex features.
class TerrainMaterial : public Material {
public:
	/// We define our biome parameters in a GPU-friendly structure
	/// This matches the layout expected by our shaders and ensures
	/// proper memory alignment for uniform buffers
	struct BiomeParameters {
		alignas(16) glm::vec4 color;
		alignas(16) glm::vec4 cliffColor;    /// Color for steep areas of this biome
		alignas(16) float minHeight;
		float maxHeight;
		float maxSteepness;    /// Maximum steepness where this biome can appear
		float cliffThreshold;  /// When to start blending in cliff material
		float roughness;       /// Surface roughness for this biome
	};

	/// Main properties structure for GPU upload
	/// We keep this separate from the class interface to maintain a clean
	/// separation between CPU and GPU data layouts. The height values here
	/// correspond to the normalized height values stored in vertex colors
	struct Properties {
		BiomeParameters biomes[4];        /// Fixed array for initial implementation
		float planetRadius;               /// Used to calculate proper height ranges
		uint32_t numBiomes;               /// Actual number of biomes in use
		float padding[2];                 /// Maintain GPU alignment
	};

	/// Create a new terrain material
	/// @param device The logical device for creating GPU resources
	/// @param name Unique identifier for this material instance
	/// @param physicalDevice Physical device for memory allocation
	TerrainMaterial(
		VkDevice device,
		const std::string& name,
		VkPhysicalDevice physicalDevice
	);

	/// Cleanup handled by destructor
	/// We use RAII principles via smart pointers for resource management
	~TerrainMaterial() override;

	/// Get the shader paths for this terrain material
	/// We override this to provide our terrain-specific shaders
	/// @return Shader paths configuration for pipeline creation
	[[nodiscard]] ShaderPaths getShaderPaths() const override;

	/// Set parameters for a specific biome
	/// Height values should match the normalized range used in vertex colors
	/// @param index Which biome to modify (0-3 in initial implementation)
	/// @param color The color for this biome
	/// @param minHeight Minimum normalized height where this biome appears (0.0 - 1.0)
	/// @param maxHeight Maximum normalized height where this biome appears (0.0 - 1.0)
	void setBiome(uint32_t index, const glm::vec4& color,
		float minHeight, float maxHeight);

	/// Set the base radius of the planet
	/// We use this for proper height calculations in the shader
	/// and to ensure biome transitions scale correctly with planet size
	/// @param radius The planet's base radius in world units
	void setPlanetRadius(float radius);

	/// Get current properties for debugging and UI
	/// @return Current terrain properties
	[[nodiscard]] const Properties& getProperties() const {
		return this->properties;
	}

protected:
	/// Create the descriptor set layout for terrain materials
	/// We override this to set up our terrain-specific uniform buffer layout
	void createDescriptorSetLayout();

	/// Create and initialize the uniform buffer
	/// This holds the material properties accessible by the shader
	void createUniformBuffer();

	/// Create descriptor pool and set
	/// These connect our uniform buffer to the shader
	void createDescriptorSet();

	/// Update the uniform buffer with current properties
	/// Called whenever material properties change
	void updateUniformBuffer();

private:
	/// Default shader paths for terrain visualization
	/// We keep these as static members since they're constant for all instances
	static constexpr const char* DefaultVertexShaderPath = "shaders/terrain.vert.spv";
	static constexpr const char* DefaultFragmentShaderPath = "shaders/terrain.frag.spv";

	/// Current material properties
	/// This holds our CPU-side copy of the shader parameters
	Properties properties;

	/// Shader paths stored for pipeline creation
	std::string vertexShaderPath;
	std::string fragmentShaderPath;
};

} /// namespace lillugsi::rendering