#version 450

/// Input from vertex shader
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragPosition;
layout(location = 3) in vec2 fragTexCoord;
layout(location = 4) in mat3 fragTBN;        /// TBN matrix for normal mapping (uses locations 4, 5, 6)
layout(location = 7) in vec3 fragViewDir;    /// View direction for specular calculations

/// Output color
layout(location = 0) out vec4 outColor;

/// Light data structure matches vertex shader
struct Light {
	vec4 direction;         /// Direction vector (w unused)
	vec4 colorAndIntensity; /// RGB color and intensity in w
	vec4 ambient;           /// Ambient color (w unused)
};

/// Light buffer (set = 1)
/// Single uniform buffer containing all lights
/// Separate set from material allows for efficient updates
layout(set = 1, binding = 0) uniform LightBuffer {
	Light lights[16];  /// Array size matches LightManager::MaxLights
} lightData;

/// PBR material properties (set = 2)
/// This set contains all material-specific parameters
layout(set = 2, binding = 0) uniform MaterialUBO {
	vec4 baseColor;        /// Base color with alpha
	float roughness;       /// Surface roughness
	float metallic;        /// Metallic factor
	float ambient;         /// Ambient occlusion
	float useAlbedoTexture; /// Whether to use the albedo texture (0.0 = no, 1.0 = yes)
	float useNormalMap;    /// Whether to use the normal map texture
	float useRoughnessMap; /// Whether to use the roughness map texture
	float useMetallicMap;  /// Whether to use the metallic map texture
	float useOcclusionMap; /// Whether to use the occlusion map texture
	float normalStrength;  /// Normal map strength factor
	float roughnessStrength; /// Roughness map strength factor
	float metallicStrength; /// Metallic map strength factor
	float occlusionStrength; /// Occlusion map strength factor
/// Texture tiling factors
	vec2 albedoTiling;     /// Tiling factor for albedo texture
	vec2 normalTiling;     /// Tiling factor for normal map
	vec2 roughnessTiling;  /// Tiling factor for roughness map
	vec2 metallicTiling;   /// Tiling factor for metallic map
	vec2 occlusionTiling;  /// Tiling factor for occlusion map
/// Channel masks for multi-channel textures
	uint roughnessChannel; /// Channel index for roughness (0=R, 1=G, 2=B, 3=A)
	uint metallicChannel;  /// Channel index for metallic
	uint occlusionChannel; /// Channel index for occlusion
} material;

/// Material textures (set = 2)
layout(set = 2, binding = 1) uniform sampler2D albedoTexture;  /// Base color/albedo texture
layout(set = 2, binding = 2) uniform sampler2D normalTexture;  /// Normal map texture
layout(set = 2, binding = 3) uniform sampler2D roughnessTexture; /// Roughness map texture
layout(set = 2, binding = 4) uniform sampler2D metallicTexture;  /// Metallic map texture
layout(set = 2, binding = 5) uniform sampler2D occlusionTexture; /// Occlusion map texture

/// Define constants used in PBR calculations
const float PI = 3.14159265359;
const float EPSILON = 0.0001; /// Small value to prevent division by zero

void main() {
	/// Normalize the interpolated normal
	/// This is necessary because linear interpolation across the triangle
	/// can result in non-unit vectors, even if the vertex normals were normalized
	vec3 normal = normalize(fragNormal);

	/// Get base color, either from texture or material uniform
	/// The useAlbedoTexture flag controls whether we use the texture or uniform value
	/// This gives artists flexibility to use either solid colors or textured surfaces
	vec3 albedo;
	if (material.useAlbedoTexture > 0.5) {
		/// Sample the albedo texture with tiling applied
		/// We apply the tiling factor to create repetition of textures across larger surfaces
		vec2 tiledTexCoord = fragTexCoord * material.albedoTiling;
		albedo = texture(albedoTexture, tiledTexCoord).rgb;
	} else {
		/// Use the material's base color directly
		/// This is useful for simple materials or when prototyping
		albedo = material.baseColor.rgb;
	}

	/// Initialize the final color with the ambient term
	/// This represents indirect light from the environment
	/// Even shadowed areas receive this minimal lighting
	vec3 finalColor = vec3(0.0);
	vec3 ambientColor = vec3(0.0);

	/// Process all lights
	/// We loop through all available lights to accumulate their contributions
	/// Each light adds both direct illumination and ambient light
	for (int i = 0; i < 16; i++) {  /// Using fixed size for simplicity in this stage
		Light light = lightData.lights[i];

		/// Skip lights with zero intensity (inactive lights)
		/// This optimization prevents unnecessary calculations for unused lights
		float lightIntensity = light.colorAndIntensity.w;
		if (lightIntensity < EPSILON) continue;

		/// Extract light color and direction
		vec3 lightColor = light.colorAndIntensity.rgb * lightIntensity;
		vec3 lightDir = normalize(-light.direction.xyz);

		/// Add ambient contribution from this light
		/// Ambient light represents indirect illumination from this light source
		/// It provides a base level of illumination to avoid completely dark shadows
		ambientColor += light.ambient.rgb * lightIntensity;

		/// Calculate diffuse term using Lambert's cosine law
		/// The max() ensures we don't get negative lighting from lights behind the surface
		/// The division by PI normalizes the Lambert BRDF to ensure energy conservation
		float NoL = max(dot(normal, lightDir), 0.0);
		vec3 diffuse = albedo / PI * NoL;

		/// Add this light's contribution to the final color
		/// Each light adds both its diffuse and ambient contribution
		finalColor += diffuse * lightColor;
	}

	/// Add accumulated ambient light
	/// We use the material's ambient occlusion factor to modulate ambient lighting
	/// This helps create more realistic shadows in crevices and occluded areas
	finalColor += ambientColor * albedo * material.ambient;

	/// Simple tone mapping (Reinhard operator)
	/// This compresses HDR values into LDR range for display
	/// We'll implement more sophisticated tone mapping in later stages
	finalColor = finalColor / (finalColor + vec3(1.0));

	/// Set output color with opaque alpha
	/// We use the alpha channel from the material's base color
	/// This preserves any transparency settings set by the artist
	outColor = vec4(finalColor, material.baseColor.a);
}