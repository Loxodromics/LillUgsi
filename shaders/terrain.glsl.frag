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

/// Height range constants
/// These define the valid range for height values and help catch errors
const float MIN_VALID_HEIGHT = 0.0;
const float MAX_VALID_HEIGHT = 1.0;

/// Debug colors for error visualization
/// Using distinct colors helps identify specific issues
const vec4 ERROR_COLOR_INVALID_RANGE = vec4(1.0, 0.0, 1.0, 1.0);  /// Magenta
const vec4 ERROR_COLOR_NO_COVERAGE = vec4(1.0, 0.5, 0.0, 1.0);    /// Orange

/// Debug visualization modes
/// These values must match the C++ TerrainDebugMode enum
const uint DEBUG_MODE_NONE = 0;
const uint DEBUG_MODE_HEIGHT = 1;
const uint DEBUG_MODE_STEEPNESS = 2;
const uint DEBUG_MODE_NORMALS = 3;
const uint DEBUG_MODE_BIOME_BOUNDARIES = 4;
const uint DEBUG_MODE_NOISE_PATTERNS_RAW = 5;       /// Raw simplex noise output
const uint DEBUG_MODE_NOISE_PATTERNS_FBM = 6;       /// FBM noise with current parameters
const uint DEBUG_MODE_NOISE_PATTERNS_COLORED = 7;   /// FBM noise with color mapping

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
	uint biomeId;          /// Unique number to identify each biome
	float padding;
};

/// Terrain material properties buffer matches CPU struct
layout(set = 2, binding = 0) uniform TerrainMaterialUBO {
	BiomeParameters biomes[4];  /// Array of biome definitions
	float planetRadius;         /// Base radius for calculations
	uint numBiomes;             /// Number of active biomes
	uint debugMode;             /// Current debug visualization mode
	float padding;              /// Keeps alignment with CPU struct
} material;

///	Simplex 3D Noise
///	by Ian McEwan, Ashima Arts
vec4 permute(vec4 x){return mod(((x*34.0)+1.0)*x, 289.0);}
vec4 taylorInvSqrt(vec4 r){return 1.79284291400159 - 0.85373472095314 * r;}

float simplexNoise(vec3 v){
	const vec2  C = vec2(1.0/6.0, 1.0/3.0) ;
	const vec4  D = vec4(0.0, 0.5, 1.0, 2.0);

	/// First corner
	vec3 i  = floor(v + dot(v, C.yyy) );
	vec3 x0 =   v - i + dot(i, C.xxx) ;

	/// Other corners
	vec3 g = step(x0.yzx, x0.xyz);
	vec3 l = 1.0 - g;
	vec3 i1 = min( g.xyz, l.zxy );
	vec3 i2 = max( g.xyz, l.zxy );

	///  x0 = x0 - 0. + 0.0 * C
	vec3 x1 = x0 - i1 + 1.0 * C.xxx;
	vec3 x2 = x0 - i2 + 2.0 * C.xxx;
	vec3 x3 = x0 - 1. + 3.0 * C.xxx;

	/// Permutations
	i = mod(i, 289.0 );
	vec4 p = permute( permute( permute(
		i.z + vec4(0.0, i1.z, i2.z, 1.0 ))
		+ i.y + vec4(0.0, i1.y, i2.y, 1.0 ))
		+ i.x + vec4(0.0, i1.x, i2.x, 1.0 )
	);

	/// Gradients
	/// ( N*N points uniformly over a square, mapped onto an octahedron.)
	float n_ = 1.0/7.0; // N=7
	vec3  ns = n_ * D.wyz - D.xzx;

	vec4 j = p - 49.0 * floor(p * ns.z *ns.z);  ///  mod(p,N*N)

	vec4 x_ = floor(j * ns.z);
	vec4 y_ = floor(j - 7.0 * x_ );    /// mod(j,N)

	vec4 x = x_ *ns.x + ns.yyyy;
	vec4 y = y_ *ns.x + ns.yyyy;
	vec4 h = 1.0 - abs(x) - abs(y);

	vec4 b0 = vec4( x.xy, y.xy );
	vec4 b1 = vec4( x.zw, y.zw );

	vec4 s0 = floor(b0)*2.0 + 1.0;
	vec4 s1 = floor(b1)*2.0 + 1.0;
	vec4 sh = -step(h, vec4(0.0));

	vec4 a0 = b0.xzyw + s0.xzyw*sh.xxyy ;
	vec4 a1 = b1.xzyw + s1.xzyw*sh.zzww ;

	vec3 p0 = vec3(a0.xy,h.x);
	vec3 p1 = vec3(a0.zw,h.y);
	vec3 p2 = vec3(a1.xy,h.z);
	vec3 p3 = vec3(a1.zw,h.w);

	/// Normalise gradients
	vec4 norm = taylorInvSqrt(vec4(dot(p0,p0), dot(p1,p1), dot(p2, p2), dot(p3,p3)));
	p0 *= norm.x;
	p1 *= norm.y;
	p2 *= norm.z;
	p3 *= norm.w;

	/// Mix final noise value
	vec4 m = max(0.6 - vec4(dot(x0,x0), dot(x1,x1), dot(x2,x2), dot(x3,x3)), 0.0);
	m = m * m;
	return 42.0 * dot( m*m, vec4( dot(p0,x0), dot(p1,x1), dot(p2,x2), dot(p3,x3) ) ) + 0.0;
}

/// Generate fractal Brownian motion noise using 3D simplex noise
/// We use FBM to create natural-looking patterns by combining multiple scales of noise
/// This creates more interesting terrain features than single-frequency noise
/// @param p Position to sample, assumed to be in world space
/// @param params Noise generation parameters controlling frequency, detail, and character
/// @return Combined noise value in range [-1, 1]
float fbm(vec3 p, NoiseParams params) {
	float value = 0.0;
	float amplitude = 1.0;
	float frequency = 1.0;
	float maxValue = 0.0;  /// Used for normalizing result

	/// Combine multiple octaves of noise
	/// Each octave adds finer detail with decreasing influence
	/// We track maxValue to ensure consistent output range regardless of parameters
	for (uint i = 0; i < params.octaves; i++) {
		/// Scale position by current frequency and base frequency
		/// This controls the size of features in the noise
		vec3 samplePos = p * frequency * params.baseFrequency;

		/// Add scaled noise contribution from this octave
		value += simplexNoise(samplePos) * amplitude;
		maxValue += amplitude;

		/// Update parameters for next octave:
		/// - Amplitude decreases by persistence to reduce influence of higher frequencies
		/// - Frequency increases by lacunarity to add finer detail
		amplitude *= params.persistence;
		frequency *= params.lacunarity;
	}

	/// Normalize result to ensure consistent range regardless of params
	/// This helps maintain predictable blending between different materials
	value = value / maxValue;

	/// Apply final amplitude scaling
	/// This controls the overall strength of the noise effect
	return value * params.amplitude;
}

/// Helper functions for biome range calculations
/// These improve code readability and maintenance by centralizing overlap logic
bool biomesOverlap(BiomeParameters a, BiomeParameters b) {
	return max(a.minHeight, b.minHeight) <= min(a.maxHeight, b.maxHeight);
}

float getOverlapStart(BiomeParameters a, BiomeParameters b) {
	return max(a.minHeight, b.minHeight);
}

float getOverlapEnd(BiomeParameters a, BiomeParameters b) {
	return min(a.maxHeight, b.maxHeight);
}

/// Safely calculate normalized position within a range
/// Handles edge cases like zero-width ranges to prevent division by zero
float getNormalizedOverlapPosition(float height, float start, float end) {
	float range = end - start;
	if (abs(range) < 0.0001) {
		return 0.5;  /// Return center point for negligible ranges
	}
	return (height - start) / range;
}

/// Calculate how much influence a biome has at a given height
/// We handle overlapping height ranges by using the overlap itself as the transition zone
/// @param biome The biome parameters to evaluate
/// @param height Normalized height value from terrain
/// @return Influence factor from 0 (no influence) to 1 (full influence)
float calculateHeightInfluence(BiomeParameters biome, float height) {
	/// For heights completely outside the biome range, no influence
	if (height < biome.minHeight || height > biome.maxHeight) {
		return 0.0;
	}

	/// For heights in the core range (not in overlap with neighbors), full influence
	float influence = 1.0;

	/// Check for overlaps with other biomes
	for (uint i = 0; i < material.numBiomes; i++) {
		BiomeParameters other = material.biomes[i];

		/// Skip self-comparison using biome IDs for safety
		if (other.biomeId == biome.biomeId) {
			continue;
		}

		/// If we overlap with another biome, calculate transition factor
		if (biomesOverlap(biome, other) && height >= other.minHeight && height <= other.maxHeight) {
			/// Calculate smooth transition through overlap region
			float overlapStart = getOverlapStart(biome, other);
			float overlapEnd = getOverlapEnd(biome, other);

			if (height < biome.maxHeight && height > other.minHeight) {
				float t = getNormalizedOverlapPosition(height, overlapStart, overlapEnd);

				/// Smooth the transition
				t = smoothstep(0.0, 1.0, t);

				/// Invert blend direction for higher biomes
				if (other.minHeight > biome.minHeight) {
					t = 1.0 - t;
				}

				influence = min(influence, t);
			}
		}
	}

	return influence;
}

/// Verify biome setup and visualize errors
/// Returns error color if issues found, otherwise returns vec4(0)
vec4 debugBiomeValidity() {
	/// Check for invalid ranges
	for (uint i = 0; i < material.numBiomes; i++) {
		if (material.biomes[i].maxHeight < material.biomes[i].minHeight) {
			return ERROR_COLOR_INVALID_RANGE;
		}
	}

	/// Check for height coverage
	bool heightCovered = false;
	for (uint i = 0; i < material.numBiomes; i++) {
		if (fragHeight >= material.biomes[i].minHeight &&
		fragHeight <= material.biomes[i].maxHeight) {
			heightCovered = true;
			break;
		}
	}

	if (!heightCovered) {
		return ERROR_COLOR_NO_COVERAGE;
	}

	return vec4(0.0);
}

/// Debug visualization of height bands and overlaps
/// This helps verify biome height ranges and transitions
vec4 debugHeightBands() {
	/// First check for setup errors
	vec4 errorColor = debugBiomeValidity();
	if (errorColor.a > 0.0) {
		return errorColor;
	}

	/// Show raw height value in grayscale if no biomes defined
	if (material.numBiomes == 0) {
		return vec4(vec3(fragHeight), 1.0);
	}

	/// Count overlaps at current height for visualization
	int overlapCount = 0;
	for (uint i = 0; i < material.numBiomes; i++) {
		for (uint j = i + 1; j < material.numBiomes; j++) {
			if (biomesOverlap(material.biomes[i], material.biomes[j]) &&
			fragHeight >= getOverlapStart(material.biomes[i], material.biomes[j]) &&
			fragHeight <= getOverlapEnd(material.biomes[i], material.biomes[j])) {
				overlapCount++;
			}
		}
	}

	/// Show overlap intensity in debug color
	if (overlapCount > 0) {
		return vec4(1.0, 1.0, 0.0, 1.0) * (float(overlapCount) / float(material.numBiomes));
	}

	/// Color bands for non-overlapping regions
	for (uint i = 0; i < material.numBiomes; i++) {
		if (fragHeight >= material.biomes[i].minHeight &&
		fragHeight <= material.biomes[i].maxHeight) {
			const vec4 debugColors[4] = vec4[4](
				vec4(1.0, 0.0, 0.0, 1.0),  // Red
				vec4(0.0, 1.0, 0.0, 1.0),  // Green
				vec4(0.0, 0.0, 1.0, 1.0),  // Blue
				vec4(1.0, 1.0, 0.0, 1.0)   // Yellow
			);
			return debugColors[i];
		}
	}

	return vec4(0.5, 0.5, 0.5, 1.0);
}

/// Blend biome colors based on height influence
/// This function handles the basic height-based blending without noise or other factors
/// Later we'll extend this to handle more complex transitions
vec4 calculateBasicBiomeColor() {
	vec4 finalColor = vec4(0.0);
	float totalInfluence = 0.0;

	/// Accumulate weighted contributions from each biome
	/// We'll later extend this to handle other influence factors
	for (uint i = 0; i < material.numBiomes; i++) {
		float influence = calculateHeightInfluence(material.biomes[i], fragHeight);

		/// Skip biomes with negligible influence
		/// This optimization will become more important as we add more factors
		if (influence > 0.01) {
			finalColor += material.biomes[i].color * influence;
			totalInfluence += influence;
		}
	}

	/// Normalize the result
	/// This ensures we maintain full opacity and correct color blending
	return finalColor / totalInfluence;
}

void main() {
	if (material.debugMode == DEBUG_MODE_NONE) {
		/// Calculate basic height-based biome color
		/// This will be the foundation for more complex blending later
		vec4 biomeColor = calculateBasicBiomeColor();

		/// For now, output raw biome color
		/// Later we'll add lighting, material properties, and other effects
		outColor = biomeColor;
		return;
	}
	else if (material.debugMode == DEBUG_MODE_NOISE_PATTERNS_RAW) {
		/// Apply global noise scale to position
		/// This allows easy adjustment of overall noise scale during development
		vec3 scaledPos = fragPosition * 2.0;//noiseDebug.noiseScale;

		/// Show raw simplex noise for basic pattern verification
		float noiseValue = simplexNoise(scaledPos);

		/// Output grayscale noise for raw/fbm modes
		/// We remap from [-1,1] to [0,1] for proper display
		outColor = vec4(vec3(noiseValue * 0.5 + 0.5), 1.0);
		return;
	}
	else if (material.debugMode == DEBUG_MODE_NOISE_PATTERNS_FBM) {
		/// Apply global noise scale to position
		/// This allows easy adjustment of overall noise scale during development
		vec3 scaledPos = fragPosition * 2.0;//noiseDebug.noiseScale;

		/// Show FBM noise to verify parameter effects
		NoiseParams params;
		params.baseFrequency = 1.0;
		params.amplitude = 1.0;
		params.octaves = 7;
		params.persistence = 0.5;
		params.lacunarity = 2.0;

		float noiseValue = fbm(scaledPos, params);
		/// Output grayscale noise for raw/fbm modes
		/// We remap from [-1,1] to [0,1] for proper display
		outColor = vec4(vec3(noiseValue * 0.5 + 0.5), 1.0);
		return;
	}
	else if (material.debugMode == DEBUG_MODE_NOISE_PATTERNS_COLORED) {
		/// Apply global noise scale to position
		/// This allows easy adjustment of overall noise scale during development
		vec3 scaledPos = fragPosition * 2.0;//noiseDebug.noiseScale;
		/// Map noise to color ramp for better visualization
		/// We use different colors to more clearly show the noise structure
		float noiseValue = fbm(scaledPos, material.biomes[0].noise);
		vec3 color = mix(
			vec3(0.0, 0.0, 0.8),  /// Dark blue for low values
			vec3(1.0, 0.0, 0.0),  /// Red for high values
			noiseValue * 0.5 + 0.5
		);

		outColor = vec4(color, 1.0);
		return;
	}

	outColor = vec4(0.0, 1.0, 0.0, 1.0);
}