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
	/// NoiseParameters defines how noise is generated and applied for each biome
	/// We separate noise parameters to:
	/// 1. Keep biome parameters organized and focused
	/// 2. Allow for easy addition of new noise features
	/// 3. Maintain clear relationships between noise parameters
	struct NoiseParameters {
		alignas(16) struct {
			float baseFrequency{2.0f};    /// Base frequency for noise sampling
			float amplitude{1.0f};        /// Overall strength of noise effect
			uint32_t octaves{4};         /// Number of noise layers to combine
			float persistence{0.5f};     /// How quickly amplitude decreases per octave
			float lacunarity{2.0f};      /// How quickly frequency increases per octave
			float padding[3];            /// Pad to 32 bytes
		};
	};

	struct TransitionParameters {
		alignas(16) struct {                /// 16 bytes total
			uint32_t type;                  /// Type of transition function to use (simplex, Worley)
			float scale;                    /// Scale of the noise pattern
			float transitionSharpness;      /// Controls edge sharpness (0: soft blend, 1: sharp cutoff)
			float padding;                  /// Pad to 16 bytes
		};
		alignas(16) NoiseParameters noise;  /// Noise parameters for the transition (32 bytes)
	};

	/// BiomeParameters now includes noise control and enhanced transition options
	/// We extend the existing structure to support more natural-looking terrain
	/// while maintaining backward compatibility
	struct BiomeParameters {
		alignas(16) glm::vec4 color;           /// Base color of the biome
		alignas(16) glm::vec4 cliffColor;      /// Color for steep areas
		alignas(16) struct {                   /// 16 bytes total
			float minHeight;                   /// Height where biome starts
			float maxHeight;                   /// Height where biome ends
			float maxSteepness;                /// Maximum steepness where biome appears
			float cliffThreshold;              /// When to start blending cliff material
		};
		alignas(16) struct {                  /// 16 bytes total
			float roughness;                  /// Base surface roughness
			float cliffRoughness;             /// Roughness for cliff areas
			float metallic;                   /// Base metallic value
			float cliffMetallic;              /// Metallic value for cliff areas
		};
		alignas(16) NoiseParameters noise;    /// Noise settings for this biome (32 bytes)
		alignas(16) struct {                  /// 16 bytes total
			uint32_t biomeId;                 /// Unique number to identify each biome in the shader
			float padding1;                   /// Pad to 16 bytes
			float padding2;
			float padding3;
		};
		alignas(16) TransitionParameters transition;  /// Parameters for transition to next biome (48 bytes)
	};

	/// Enum for transition types - must match shader values
	enum class TransitionType : uint32_t {
		Simplex = 0,
		Worley = 1
	};

	/// Extended Properties structure to include debug support
	/// We maintain the same memory layout as before but add new functionality
	struct Properties {
		alignas(16) BiomeParameters biomes[4];  /// Fixed array for initial implementation
		alignas(16) struct {                    /// 16 bytes total
			float planetRadius{1.0f};           /// Used to calculate proper height ranges
			uint32_t numBiomes{0};              /// Actual number of biomes in use
			uint32_t debugMode{0};              /// Current debug visualization mode
			float padding{0.0f};                /// Pad to 16 bytes
		};
	};

	/// Debug modes for terrain visualization
	/// We use an enum class for type safety and clear intent
	enum class TerrainDebugMode : uint32_t {
		None = 0,            /// Normal rendering
		Height,              /// Show raw height values
		Steepness,           /// Show slope calculations
		Normals,             /// Visualize surface normals
		BiomeBoundaries,     /// Show raw biome transitions
		NoisePatternsRaw,    /// Raw simplex noise output
		NoisePatternsFBM,    /// FBM noise with current parameters
		NoisePatternsColored /// FBM noise with color mapping
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

	/// Set noise parameters for a specific biome
	/// We provide this method to allow fine-tuning of noise behavior per biome
	/// @param index Which biome to modify
	/// @param params New noise parameters for the biome
	void setNoiseParameters(uint32_t index, const NoiseParameters& params);

	/// Set the debug visualization mode
	/// We use this for development and tuning of the terrain system
	/// @param mode The debug mode to enable
	void setDebugMode(TerrainDebugMode mode);

	/// Get current debug mode
	/// @return The active debug visualization mode
	[[nodiscard]] TerrainDebugMode getDebugMode() const {
		return static_cast<TerrainDebugMode>(this->properties.debugMode);
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