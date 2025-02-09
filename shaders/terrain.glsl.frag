#version 450

/// Input from vertex shader
layout(location = 0) in vec3 fragPosition;   /// World-space position
layout(location = 1) in vec3 fragNormal;     /// World-space normal
layout(location = 2) in float fragHeight;    /// Normalized height value
layout(location = 3) in float fragSteepness; /// Terrain steepness

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

/// Constants for PBR lighting calculations
/// These values help create physically plausible results
const float PI = 3.14159265359;
const float MIN_ROUGHNESS = 0.04; /// Prevent perfect smoothness for realism

/// Combined material properties for a terrain point
/// We group these properties to make the relationship between them clear
/// and to simplify passing them between shader functions
struct TerrainMaterial {
	vec4 color;           /// Final color after biome blending
	float roughness;      /// Surface micro-roughness
	float metallic;       /// Metallic content of the surface
};

/// Update biome parameters structure to match C++ definition
struct BiomeParameters {
	vec4 color;           /// Base color of the biome
	vec4 cliffColor;      /// Color for steep areas
	float minHeight;      /// Height where biome starts
	float maxHeight;      /// Height where biome ends
	float maxSteepness;   /// Maximum steepness where biome appears
	float cliffThreshold; /// When to start blending cliff material
	float roughness;      /// Base surface roughness
	float cliffRoughness; /// Roughness for cliff areas
	float metallic;       /// Base metallic value
	float cliffMetallic;  /// Metallic value for cliff areas
};

/// Terrain material properties (set = 2)
/// Contains all biome data for height-based coloring
layout(set = 2, binding = 0) uniform TerrainMaterialUBO {
	BiomeParameters biomes[4];  /// Array of biome definitions
	float planetRadius;         /// Base radius for calculations
	uint numBiomes;             /// Number of active biomes
	float padding[2];           /// Keeps alignment with CPU struct
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

/// Calculate interpolated material properties for a point on the terrain
/// We separate this from the main biome calculation to keep the code focused
/// on one aspect of the material computation
TerrainMaterial calculateMaterialProperties(float height, float steepness) {
	TerrainMaterial result;
	result.color = vec4(0.0);
	result.roughness = 0.0;
	result.metallic = 0.0;
	float totalWeight = 0.0;

	/// Calculate contribution from each active biome
	for (uint i = 0; i < material.numBiomes; i++) {
		BiomeParameters biome = material.biomes[i];

		/// Calculate how much this height belongs to this biome
		/// We use smoothstep for smooth transitions at biome boundaries
		float weight = 1.0;

		/// Handle height-based blending between biomes
		if (i > 0) {
			BiomeParameters prevBiome = material.biomes[i - 1];
			weight *= smoothstep(
			biome.minHeight,
			prevBiome.maxHeight,
			height
			);
		}

		if (i < material.numBiomes - 1) {
			BiomeParameters nextBiome = material.biomes[i + 1];
			weight *= 1.0 - smoothstep(
			nextBiome.minHeight,
			biome.maxHeight,
			height
			);
		}

		/// Calculate cliff blend based on steepness
		/// This determines how much of the cliff color to show
		float cliffBlend = smoothstep(
		biome.cliffThreshold,
		biome.maxSteepness,
		steepness
		);

		/// Blend between regular and cliff properties
		vec4 biomeColor = mix(biome.color, biome.cliffColor, cliffBlend);
		float biomeRoughness = mix(biome.roughness, biome.cliffRoughness, cliffBlend);
		float biomeMetallic = mix(biome.metallic, biome.cliffMetallic, cliffBlend);

		/// Accumulate weighted properties
		result.color += biomeColor * weight;
		result.roughness += biomeRoughness * weight;
		result.metallic += biomeMetallic * weight;
		totalWeight += weight;
	}

	/// Normalize results
	if (totalWeight > 0.0) {
		result.color /= totalWeight;
		result.roughness /= totalWeight;
		result.metallic /= totalWeight;
	} else {
		/// Error state - use obvious colors and values
		result.color = vec4(1.0, 0.0, 1.0, 1.0);
		result.roughness = 1.0;
		result.metallic = 0.0;
	}

	return result;
}

/// Calculate how much light is reflected vs. refracted
/// This is a key principle of PBR - energy conservation
vec3 calculateFresnelSchlick(float cosTheta, vec3 F0) {
	/// Fresnel-Schlick approximation
	/// As viewing angle becomes more grazing (cosTheta approaches 0),
	/// more light is reflected rather than refracted
	return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

/// Calculate how rough the surface appears at this specific viewing angle
/// This accounts for the apparent smoothing of rough surfaces at grazing angles
float calculateGeometrySmith(float NdotV, float NdotL, float roughness) {
	/// We square the roughness to get more intuitive artist control
	float r = (roughness + 1.0);
	float k = (r * r) / 8.0;

	/// Calculate geometry obstruction (view direction) and shadowing (light direction)
	float ggx1 = NdotV / (NdotV * (1.0 - k) + k);
	float ggx2 = NdotL / (NdotL * (1.0 - k) + k);

	/// Combine for total geometry term
	return ggx1 * ggx2;
}

/// Calculate microfacet distribution
/// This determines how many surface microfacets are aligned to reflect light
/// toward the viewer at this specific angle
float calculateNormalDistribution(float NdotH, float roughness) {
	float a      = roughness * roughness;
	float a2     = a * a;
	float NdotH2 = NdotH * NdotH;

	float num   = a2;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;

	return num / denom;
}

/// Calculate PBR lighting for a directional light
/// This implements the full PBR lighting model while maintaining
/// compatibility with our existing light structure
vec3 calculatePBRLighting(Light light, vec3 normal, vec3 baseColor, float roughness, float metallic) {
	vec3 lightDir = -normalize(light.direction.xyz);
	vec3 viewDir = normalize(-fragPosition); /// Camera is at origin in view space
	vec3 halfVector = normalize(lightDir + viewDir);

	/// Calculate basic dot products we'll need
	float NdotL = max(dot(normal, lightDir), 0.0);
	float NdotV = max(dot(normal, viewDir), 0.0);
	float NdotH = max(dot(normal, halfVector), 0.0);
	float HdotV = max(dot(halfVector, viewDir), 0.0);

	/// Ensure minimum roughness for physical plausibility
	roughness = max(roughness, MIN_ROUGHNESS);

	/// Calculate specular reflection ratio
	/// For metals, this is tinted by the base color
	/// For non-metals (dialectrics), this is plain white
	vec3 F0 = mix(vec3(0.04), baseColor, metallic);

	/// Calculate all lighting terms
	float NDF = calculateNormalDistribution(NdotH, roughness);
	float G   = calculateGeometrySmith(NdotV, NdotL, roughness);
	vec3 F    = calculateFresnelSchlick(HdotV, F0);

	/// Combine for specular term
	/// Now F is a vec3, so the result will be a vec3 as well
	vec3 specular = (NDF * G * F) / max(4.0 * NdotV * NdotL, 0.001);

	/// Calculate diffuse contribution
	/// Metals have no diffuse component
	vec3 diffuse = (1.0 - F) * (1.0 - metallic) * baseColor / PI;

	/// Combine everything with light color and intensity
	vec3 lightColor = light.colorAndIntensity.rgb * light.colorAndIntensity.a;
	vec3 directLight = (diffuse + specular) * lightColor * NdotL;

	/// Add ambient contribution
	vec3 ambient = light.ambient.rgb * baseColor;

	return ambient + directLight;
}

void main() {
	/// Normalize the interpolated normal
	vec3 normal = normalize(fragNormal);

	/// Get all material properties for this point
	TerrainMaterial terrainMat = calculateMaterialProperties(fragHeight, fragSteepness);

	vec3 finalColor = vec3(0.0);

	/// Accumulate lighting using PBR parameters
	for (int i = 0; i < 16; ++i) {  /// MaxLights from C++ code
		if (lightData.lights[i].colorAndIntensity.a > 0.0) {
			/// Use both diffuse color and PBR parameters for lighting
			finalColor += calculatePBRLighting(
			lightData.lights[i],
			normal,
			terrainMat.color.rgb,
			terrainMat.roughness,
			terrainMat.metallic
			);
		}
	}

	/// Apply a simple tone mapping to prevent over-saturation
	finalColor = finalColor / (finalColor + vec3(1.0));

	/// Output final color with alpha from material
	outColor = vec4(finalColor, terrainMat.color.a);
}