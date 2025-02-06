#version 450

/// Input from vertex shader
layout(location = 0) in vec3 fragPosition;  /// World-space position
layout(location = 1) in vec3 fragNormal;    /// World-space normal
layout(location = 2) in float fragHeight;   /// Normalized height value

/// Output color
layout(location = 0) out vec4 outColor;

/// Light data structure matches vertex shader
struct Light {
	vec4 direction;         /// Direction vector (w unused)
	vec4 colorAndIntensity; /// RGB color and intensity in w
	vec4 ambient;           /// Ambient color (w unused)
};

/// Light buffer (set = 1)
/// Shared across all materials for consistent lighting
layout(set = 1, binding = 0) uniform LightBuffer {
	Light lights[16];      /// Array size matches LightManager::MaxLights
} lightData;

/// Biome parameters structure
/// Matches BiomeParameters in TerrainMaterial class
struct BiomeParameters {
	vec4 color;           /// Base color of the biome
	float minHeight;      /// Height where biome starts
	float maxHeight;      /// Height where biome ends
};

/// Terrain material properties (set = 2)
/// Contains all biome data for height-based coloring
layout(set = 2, binding = 0) uniform TerrainMaterialUBO {
	BiomeParameters biomes[4];  /// Array of biome definitions
	float planetRadius;         /// Base radius for calculations
	float padding[3];           /// Keeps alignment with CPU struct
} material;

/// Calculate contribution from a single directional light
/// We separate this calculation to make the lighting logic clearer
/// and to allow for easy modification of the lighting model
vec3 calculateDirectionalLight(Light light, vec3 normal, vec3 baseColor) {
	vec3 lightDir = -normalize(light.direction.xyz);
	vec3 lightColor = light.colorAndIntensity.rgb;
	float lightIntensity = light.colorAndIntensity.a;

	/// Calculate diffuse contribution
	float diff = max(dot(normal, lightDir), 0.0);
	vec3 diffuse = diff * lightColor * lightIntensity;

	/// Add ambient contribution
	vec3 ambient = light.ambient.rgb;

	/// Return combined lighting
	return baseColor * (ambient + diffuse);
}

/// Calculate biome color based on height
/// We smoothly interpolate between biomes to avoid hard transitions
vec4 calculateBiomeColor(float height) {
	vec4 finalColor = vec4(0.0);
	float totalWeight = 0.0;

	/// Calculate contribution from each biome
	for (int i = 0; i < 4; i++) {
		BiomeParameters biome = material.biomes[i];

		/// Calculate how much this height belongs to this biome
		/// We use smoothstep for smooth transitions at biome boundaries
		float weight = 1.0;

		/// For all biomes, blend with previous if available
		if (i > 0) {
			BiomeParameters prevBiome = material.biomes[i - 1];
			weight *= smoothstep(
			biome.minHeight,
			prevBiome.maxHeight,
			height
			);
		}

		/// For all biomes, blend with next if available
		if (i < 3) {
			BiomeParameters nextBiome = material.biomes[i + 1];
			weight *= 1.0 - smoothstep(
			nextBiome.minHeight,
			biome.maxHeight,
			height
			);
		}

		/// Accumulate weighted colors
		finalColor += biome.color * weight;
		totalWeight += weight;
	}

	/// Normalize the result
	/// This ensures we always get a valid color even with overlapping biomes
	/// Put pink on error
	return totalWeight > 0.0 ? finalColor / totalWeight : vec4(1.0, 0.0, 1.0, 1.0);
}

void main() {
	/// Normalize the interpolated normal
	vec3 normal = normalize(fragNormal);

	/// Get the base color from biome calculation
	vec4 biomeColor = calculateBiomeColor(fragHeight);

	vec3 finalColor = vec3(0.0);

	/// Accumulate lighting from all active lights
	/// We use the same lighting model as PBR shader for consistency
	for (int i = 0; i < 16; ++i) {  /// MaxLights from C++ code
		/// Only process lights with non-zero intensity
		if (lightData.lights[i].colorAndIntensity.a > 0.0) {
			finalColor += calculateDirectionalLight(
			lightData.lights[i],
			normal,
			biomeColor.rgb
			);
		}
	}

	/// Apply a simple tone mapping to prevent over-saturation
	/// This helps maintain consistent lighting with other materials
	finalColor = finalColor / (finalColor + vec3(1.0));

	/// Output final color with alpha from biome
	outColor = vec4(finalColor, biomeColor.a);
//	outColor = material.biomes[1].color;
}