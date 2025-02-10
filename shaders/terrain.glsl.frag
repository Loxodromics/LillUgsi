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

/// Noise parameters for controlling noise generation
/// These match the CPU-side NoiseParameters structure
struct NoiseParams {
	float baseFrequency;       /// Base frequency for noise sampling
	float amplitude;           /// Overall strength of noise effect
	uint octaves;             /// Number of noise layers to combine
	float persistence;        /// How quickly amplitude decreases per octave
	float lacunarity;         /// How quickly frequency increases per octave
	float padding1;
	float padding2;
	float padding3;
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
	NoiseParams noise;    /// Noise settings for this biome
	float transitionNoise; /// 0.0 = smooth, 1.0 = fully noisy transition
	float transitionScale; /// Scale of noise pattern in transitions
	float padding1;
	float padding2;
};

/// Terrain material properties buffer matches CPU struct
layout(set = 2, binding = 0) uniform TerrainMaterialUBO {
	BiomeParameters biomes[4];  /// Array of biome definitions
	float planetRadius;         /// Base radius for calculations
	uint numBiomes;             /// Number of active biomes
	uint debugMode;             /// Current debug visualization mode
	float padding;              /// Keeps alignment with CPU struct
} material;

/// Debug visualization modes
/// These values must match the C++ TerrainDebugMode enum
const uint DEBUG_MODE_NONE = 0;
const uint DEBUG_MODE_HEIGHT = 1;
const uint DEBUG_MODE_STEEPNESS = 2;
const uint DEBUG_MODE_NORMALS = 3;
const uint DEBUG_MODE_BIOME_BOUNDARIES = 4;
const uint DEBUG_MODE_NOISE_PATTERNS = 5;

/// Noise function section
/// We separate noise functions in a dedicated section to:
/// 1. Make them easy to find and modify
/// 2. Keep the main shader logic clean
/// 3. Allow for easy performance profiling

/// Generate a pseudo-random value from a 2D position
/// We use this as a basis for more complex noise functions
/// The constants are chosen for good distribution and minimal patterns
float hash2D(vec2 p) {
	const vec2 magic = vec2(0.1031, 0.1030);
	p = fract(p * magic);
	p += dot(p, p.yx + 33.33);
	return fract((p.x + p.y) * p.x);
}

/// Generate a 2D value noise
/// This is our basic noise building block, providing smooth random variation
/// @param p Position to sample noise at
/// @return Noise value in range [0, 1]
float valueNoise(vec2 p) {
	/// Get cell corners for bilinear interpolation
	vec2 i = floor(p);
	vec2 f = fract(p);

	/// Sample random values at cell corners
	float a = hash2D(i);
	float b = hash2D(i + vec2(1.0, 0.0));
	float c = hash2D(i + vec2(0.0, 1.0));
	float d = hash2D(i + vec2(1.0, 1.0));

	/// Smooth interpolation using quintic curve
	/// We use quintic instead of simple smoothstep for better visual quality
	vec2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);

	/// Interpolate between corner values
	return mix(mix(a, b, u.x),
	mix(c, d, u.x), u.y);
}

/// Generate gradient noise (improved Perlin)
/// This provides more natural-looking variation than value noise
/// @param p Position to sample noise at
/// @return Noise value in range [-1, 1]
float gradientNoise(vec2 p) {
	/// Get cell corners
	vec2 i = floor(p);
	vec2 f = fract(p);

	/// Generate random gradients for corners
	/// We use hash function to create consistent random vectors
	vec2 ga = normalize(vec2(hash2D(i) * 2.0 - 1.0,
	hash2D(i + 0.1) * 2.0 - 1.0));
	vec2 gb = normalize(vec2(hash2D(i + vec2(1.0, 0.0)) * 2.0 - 1.0,
	hash2D(i + vec2(1.0, 0.0) + 0.1) * 2.0 - 1.0));
	vec2 gc = normalize(vec2(hash2D(i + vec2(0.0, 1.0)) * 2.0 - 1.0,
	hash2D(i + vec2(0.0, 1.0) + 0.1) * 2.0 - 1.0));
	vec2 gd = normalize(vec2(hash2D(i + vec2(1.0, 1.0)) * 2.0 - 1.0,
	hash2D(i + vec2(1.0, 1.0) + 0.1) * 2.0 - 1.0));

	/// Calculate dot products with gradients
	float va = dot(ga, f);
	float vb = dot(gb, f - vec2(1.0, 0.0));
	float vc = dot(gc, f - vec2(0.0, 1.0));
	float vd = dot(gd, f - vec2(1.0, 1.0));

	/// Interpolate using quintic curve
	vec2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);

	return mix(mix(va, vb, u.x),
	mix(vc, vd, u.x), u.y);
}

/// Generate Worley (cellular) noise
/// This creates organic-looking patterns perfect for rocky terrain
/// @param p Position to sample noise at
/// @return Noise value in range [0, 1]
float worleyNoise(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);

	float minDist = 1.0;

	/// Check surrounding cells to find nearest feature point
	/// We use a 3x3 grid for good quality while maintaining performance
	for(float y = -1.0; y <= 1.0; y++) {
		for(float x = -1.0; x <= 1.0; x++) {
			vec2 neighbor = vec2(x, y);

			/// Generate random feature point in cell
			vec2 point = neighbor + vec2(
			hash2D(i + neighbor) * 0.5 + 0.25,
			hash2D(i + neighbor + 0.1) * 0.5 + 0.25
			);

			/// Calculate distance to feature point
			float dist = length(point - f);
			minDist = min(minDist, dist);
		}
	}

	return minDist;
}

/// Generate fractal brownian motion (fBm) noise
/// This combines multiple octaves of noise for natural detail at different scales
/// @param p Position to sample
/// @param noiseFunc Function pointer for base noise (value, gradient, or Worley)
/// @param octaves Number of noise layers to combine
/// @param persistence How quickly amplitude decreases per octave
/// @param lacunarity How quickly frequency increases per octave
/// @return Combined noise value
float fbm(vec2 p, NoiseParams params) {
	float value = 0.0;
	float amplitude = 1.0;
	float frequency = 1.0;
	float maxValue = 0.0;  /// Used for normalizing result

	/// Combine multiple octaves of noise
	/// Each octave adds finer detail with decreasing influence
	for(int i = 0; i < params.octaves; i++) {
		/// We use value noise as our base but could be changed
		value += valueNoise(p * frequency) * amplitude;

		maxValue += amplitude;
		amplitude *= params.persistence;
		frequency *= params.lacunarity;
	}

	/// Normalize result to [0, 1] range
	return value / maxValue;
}

/// Get a color visualization for debug purposes
/// Each debug mode provides different insights into the noise generation
/// @param worldPos The world position for noise sampling
/// @param height The normalized height value
/// @param steepness The terrain steepness
/// @return Debug visualization color
vec4 getDebugVisualization(vec3 worldPos, float height, float steepness) {
	/// Handle different debug modes
	switch (material.debugMode) {
		case DEBUG_MODE_NOISE_PATTERNS: {
			/// Show different noise patterns in RGB channels
			/// This helps verify noise function behavior and parameters

			/// Sample position based on world coordinates
			/// We scale by planet radius to maintain consistent noise scale
			vec2 noisePos = worldPos.xz / material.planetRadius;

			/// Red channel: Value noise showing basic pattern
			float valuePattern = valueNoise(noisePos * 5.0);

			/// Green channel: FBM noise showing multi-octave effect
			/// We use current biome's parameters to verify settings
			int biomeIndex = int(height * float(material.numBiomes));
			float fbmPattern = fbm(noisePos, material.biomes[biomeIndex].noise);

			/// Blue channel: Worley noise showing cellular pattern
			float worleyPattern = worleyNoise(noisePos * 3.0);

			return vec4(valuePattern, fbmPattern, worleyPattern, 1.0);
		}

		case DEBUG_MODE_BIOME_BOUNDARIES: {
			/// Show biome transition zones with noise influence
			/// This helps verify transition parameters and blending

			/// Find current biome transitions
			for (uint i = 0; i < material.numBiomes - 1; i++) {
				BiomeParameters lower = material.biomes[i];
				BiomeParameters upper = material.biomes[i + 1];

				/// Check if we're in a transition zone
				if (height >= lower.maxHeight && height <= upper.minHeight) {
					/// Calculate transition factors
					float normalizedHeight = (height - lower.maxHeight) /
					(upper.minHeight - lower.maxHeight);

					/// Sample transition noise
					vec2 noisePos = worldPos.xz * lower.transitionScale;
					float transitionNoise = valueNoise(noisePos);

					/// Show transition zone in yellow, with noise as brightness
					return vec4(1.0, 1.0, 0.0, 1.0) *
					mix(0.5, 1.0, transitionNoise);
				}
			}

			/// Outside transition zones, show regular height bands
			return vec4(vec3(fract(height * 4.0)), 1.0);
		}

		case DEBUG_MODE_HEIGHT: {
			/// Simple height visualization
			/// This helps verify height data is correct
			return vec4(vec3(height), 1.0);
		}

		case DEBUG_MODE_STEEPNESS: {
			/// Steepness visualization with enhanced contrast
			/// This helps identify cliffs and slopes
			/// We use a non-linear mapping to better show subtle variations
			float enhancedSteepness = pow(steepness, 0.5);
			return vec4(vec3(enhancedSteepness), 1.0);
		}

		case DEBUG_MODE_NORMALS: {
			/// Normal vector visualization
			/// This helps verify normal calculation
			/// We transform from [-1,1] to [0,1] range for display
			return vec4(fragNormal * 0.5 + 0.5, 1.0);
		}

		default:
		/// If no debug mode active, signal to use normal rendering
		return vec4(0.0);
	}
}

/// Simple noise test visualization
/// This function provides a raw view of noise patterns
/// @param worldPos Position for noise sampling
/// @return Color showing noise pattern
vec4 getNoiseTestVisualization(vec3 worldPos) {
	/// Scale position by planet radius for consistent noise scale
	vec2 noisePos = worldPos.xz / material.planetRadius;

	/// Generate different types of noise
	/// We combine them to show how they work together
	float basic = valueNoise(noisePos * 5.0);
	float detail = worleyNoise(noisePos * 10.0);

	/// Sample FBM with current biome parameters
	/// This shows how parameters affect the final noise
	int biomeIndex = int(clamp(fragHeight * float(material.numBiomes),
	0.0, float(material.numBiomes) - 1.0));
	float fbmNoise = fbm(noisePos, material.biomes[biomeIndex].noise);

	/// Combine for visualization:
	/// Red: Basic noise pattern
	/// Green: Detail noise
	/// Blue: FBM result
	/// This helps understand how each component contributes
	return vec4(basic, detail * 0.5, fbmNoise, 1.0);
}

void main() {
	/// Calculate basic terrain properties first
	float height = fragHeight;
	float steepness = fragSteepness;

	/// Check for debug visualization
	vec4 debugColor = getDebugVisualization(fragPosition, height, steepness);
	if (debugColor.a > 0.0) {
		outColor = debugColor;
		return;
	}

	/// Normal rendering will go here later
	/// For now, show noise test pattern
	outColor = getNoiseTestVisualization(fragPosition);
//		outColor = vec4(1.0, 0.0, 0.0, 1.0);
}

/// Calculate complete noise contribution for a terrain position
/// This combines different noise types based on biome and height
/// @param worldPos Position in world space
/// @param biomeParams Parameters for current biome
/// @return Final noise value for surface variation
float calculateTerrainNoise(vec3 worldPos, BiomeParameters biome) {
	/// Convert 3D position to 2D noise coordinates
	/// We use xz plane for horizontal variation
	vec2 noisePos = worldPos.xz * biome.noise.baseFrequency;

	/// Start with primary terrain variation
	float mainNoise = fbm(noisePos, biome.noise);

	/// Add detail noise for surface roughness
	/// We use Worley noise for rocky features and higher frequencies
	float detailNoise = worleyNoise(noisePos * 2.0) * 0.5;

	/// Combine noises with biome-specific weighting
	return mix(mainNoise, detailNoise, biome.noise.amplitude);
}

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

void main2() {
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