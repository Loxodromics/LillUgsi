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
} material;

/// Material textures (set = 2)
layout(set = 2, binding = 1) uniform sampler2D albedoTexture;  /// Base color/albedo texture
layout(set = 2, binding = 2) uniform sampler2D normalTexture;  /// Normal map texture
layout(set = 2, binding = 3) uniform sampler2D roughnessTexture; /// Roughness map texture
layout(set = 2, binding = 4) uniform sampler2D metallicTexture;  /// Metallic map texture
layout(set = 2, binding = 5) uniform sampler2D occlusionTexture; /// Occlusion map texture

const float PI = 3.14159265359;

/// Define constants used in PBR calculations
#define PI 3.14159265359
#define EPSILON 0.0001 /// Small value to prevent division by zero

/// Calculate the Normal Distribution Function using GGX/Trowbridge-Reitz distribution
/// This models the statistical distribution of microfacets on the surface
/// @param normal The surface normal
/// @param halfway The halfway vector between view and light
/// @param roughness The surface roughness parameter [0,1]
/// @return The NDF value representing microfacet alignment probability
float distributionGGX(vec3 normal, vec3 halfway, float roughness) {
	/// Square the roughness to provide more intuitive artist control
	/// Linear roughness feels inconsistent at different values, squared provides better visual mapping
	float alpha = roughness * roughness;
	float alphaSqr = alpha * alpha;

	/// Calculate how well the halfway vector aligns with the surface normal
	float NdotH = max(dot(normal, halfway), 0.0);
	float NdotH2 = NdotH * NdotH;

	/// Compute the GGX distribution
	/// This gives the statistical probability that microfacets are oriented along the halfway vector
	/// The denominator creates the characteristic "long tail" of GGX highlights
	float denominator = (NdotH2 * (alphaSqr - 1.0) + 1.0);
	denominator = PI * denominator * denominator;

	/// Return the normalized distribution value
	/// We add an epsilon to prevent division by zero for perfectly smooth surfaces
	return alphaSqr / max(denominator, EPSILON);
}

/// Calculate the Schlick-GGX Geometry Function for a single vector
/// This computes self-shadowing from microfacets along one direction (view or light)
/// @param NdotX Dot product between normal and the direction vector
/// @param roughness The surface roughness parameter [0,1]
/// @return Geometry term for the given direction
float geometrySchlickGGX(float NdotX, float roughness) {
	/// Remapping roughness for the geometry term
	/// For direct lighting, we use this remapping to account for the different behavior
	/// of geometry shadowing compared to the normal distribution function
	float r = (roughness + 1.0);
	float k = (r * r) / 8.0;

	/// Calculate the shadowing term
	/// This represents how much light is blocked by microfacets
	/// Higher roughness values lead to more self-shadowing
	float numerator = NdotX;
	float denominator = NdotX * (1.0 - k) + k;

	/// Return the geometry term
	/// Clamped to prevent division by zero
	return numerator / max(denominator, EPSILON);
}

/// Calculate the Smith model for combined geometry shadowing/masking
/// The Smith model combines shadowing from both view and light directions
/// @param normal The surface normal
/// @param view The view direction
/// @param light The light direction
/// @param roughness The surface roughness parameter [0,1]
/// @return Combined geometry term for both directions
float geometrySmith(vec3 normal, vec3 view, vec3 light, float roughness) {
	/// Calculate geometry term for both directions
	/// We compute how much light is obscured for both the incoming and outgoing directions
	float NdotV = max(dot(normal, view), 0.0);
	float NdotL = max(dot(normal, light), 0.0);

	/// Use Schlick-GGX approximation for each direction
	float ggx1 = geometrySchlickGGX(NdotV, roughness);
	float ggx2 = geometrySchlickGGX(NdotL, roughness);

	/// Combine terms using Smith method
	/// The combined term handles correlations between viewing and light directions
	return ggx1 * ggx2;
}

/// Calculate Fresnel reflectance using Schlick's approximation
/// This determines how much light is reflected vs. refracted based on view angle
/// @param cosTheta Cosine of angle between halfway vector and view direction
/// @param F0 Surface reflection at zero incidence (straight-on viewing angle)
/// @return The Fresnel reflectance
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
	/// Schlick's approximation to Fresnel equation
	/// This is a simple but effective approximation to the full Fresnel equations
	/// At grazing angles (cosTheta near 0), all surfaces approach 100% reflectivity
	return F0 + (1.0 - F0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
}

/// Calculate contribution from a single directional light using Cook-Torrance BRDF
/// This function implements physically-based lighting using the Cook-Torrance microfacet BRDF
/// @param light The light source data (direction, color, intensity)
/// @param normal Surface normal in world space
/// @param albedo Surface base color
/// @param viewDir View direction (normalized vector toward camera)
/// @param roughness Surface roughness (controls microfacet distribution)
/// @param metallic Surface metalness (controls specular response)
/// @return Final lit color including diffuse and specular components
vec3 calculateDirectionalLight(Light light, vec3 normal, vec3 albedo, vec3 viewDir, float roughness, float metallic) {
	/// Extract light properties from the light structure
	vec3 lightDir = -normalize(light.direction.xyz);
	vec3 lightColor = light.colorAndIntensity.rgb;
	float lightIntensity = light.colorAndIntensity.a;

	/// Calculate essential dot products used throughout the BRDF
	float NdotL = max(dot(normal, lightDir), 0.0);
	float NdotV = max(dot(normal, viewDir), EPSILON); /// Using small epsilon to prevent divide-by-zero

	/// Early exit for surfaces facing away from the light source
	/// This optimization skips expensive calculations when the surface can't directly see the light
	if (NdotL <= 0.0) {
		/// Return only ambient contribution when surface faces away from light
		return albedo * light.ambient.rgb;
	}

	/// Calculate the halfway vector between view and light directions
	/// The halfway vector represents the surface normal that would perfectly reflect light to the viewer
	vec3 halfwayVector = normalize(lightDir + viewDir);
	float HdotV = max(dot(halfwayVector, viewDir), 0.0);

	/// Define the surface's specular color (F0)
	/// For dielectrics (non-metals), this is a constant 0.04
	/// For metals, we use the albedo color itself, controlled by metallic parameter
	vec3 F0 = vec3(0.04);
	F0 = mix(F0, albedo, metallic);

	/// Calculate the three components of the Cook-Torrance BRDF:
	/// 1. Normal Distribution Function (D) - Statistical distribution of microfacets
	float D = distributionGGX(normal, halfwayVector, roughness);

	/// 2. Fresnel Term (F) - Reflectivity that varies with viewing angle
	vec3 F = fresnelSchlick(HdotV, F0);

	/// 3. Geometry Term (G) - Self-shadowing of microfacets
	float G = geometrySmith(normal, viewDir, lightDir, roughness);

	/// Calculate the Cook-Torrance specular BRDF
	/// The complete specular BRDF consists of the distribution, fresnel, and geometry terms
	/// divided by the normalization factor (4 * NdotV * NdotL)
	vec3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, EPSILON);

	/// Calculate the diffuse component using Lambert
	/// Lambert diffuse is simple but effective for most non-specialized materials
	/// Normalized by PI to ensure energy conservation
	vec3 diffuse = albedo / PI;

	/// Apply energy conservation
	/// As surfaces become more reflective (higher F) or more metallic,
	/// the diffuse component should decrease to conserve energy
	vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);

	/// Combine diffuse and specular components, modulated by light properties
	/// Scale by NdotL to account for light incident angle
	vec3 finalColor = (kD * diffuse + specular) * lightColor * lightIntensity * NdotL;

	/// Add ambient contribution
	/// This provides a base level of illumination representing light bounced from the environment
	finalColor += albedo * light.ambient.rgb;

	return finalColor;
}

void main() {
	/// Get base normal from vertex attributes
	vec3 normal = normalize(fragNormal);

	/// Apply normal mapping if enabled
	if (material.useNormalMap > 0.5) {
		/// Sample the normal map
		vec3 normalMap = texture(normalTexture, fragTexCoord).rgb;

		/// Convert from [0,1] to [-1,1] range
		normalMap = normalMap * 2.0 - 1.0;

//		normalMap.xy = -normalMap.xy;
//		normalMap.xyz = -normalMap.xyz;

		/// Apply normal strength factor
		/// This allows control over the intensity of the normal map effect
		/// A value of 0 would use the original normal, 1 uses the full normal map
		normalMap.xy *= material.normalStrength;

		/// Make sure the Z component is positive (pointing outward)
		/// This recalculation maintains the length of the normal
		normalMap.z = sqrt(1.0 - min(1.0, dot(normalMap.xy, normalMap.xy)));

		/// Transform the normal from tangent space to world space using the TBN matrix
		normal = normalize(fragTBN * normalMap);
	}

	/// Sample albedo texture if enabled, otherwise use the base color
	vec3 albedo;
	if (material.useAlbedoTexture > 0.5) {
		/// Sample the texture using the interpolated texture coordinates
		vec4 texColor = texture(albedoTexture, fragTexCoord);

		/// Combine texture with vertex color and material base color
		/// This allows for tinting textures with the material color
		albedo = texColor.rgb * fragColor * material.baseColor.rgb;
	} else {
		/// If no texture is used, fall back to the original behavior
		albedo = fragColor * material.baseColor.rgb;
	}

	/// Sample and apply roughness map if enabled
	float roughnessValue = material.roughness;
	if (material.useRoughnessMap > 0.5) {
		/// Sample roughness texture - typically stored in R channel
		float texRoughness = texture(roughnessTexture, fragTexCoord).r;

		/// Blend between base roughness and texture value based on strength
		roughnessValue = mix(material.roughness, texRoughness, material.roughnessStrength);
	}

	/// Sample and apply metallic map if enabled
	float metallicValue = material.metallic;
	if (material.useMetallicMap > 0.5) {
		/// Sample metallic texture - typically stored in R channel
		float texMetallic = texture(metallicTexture, fragTexCoord).r;

		/// Blend between base metallic and texture value based on strength
		metallicValue = mix(material.metallic, texMetallic, material.metallicStrength);
	}

	/// Sample and apply occlusion map if enabled
	float occlusionValue = material.ambient;
	if (material.useOcclusionMap > 0.5) {
		/// Sample occlusion texture - typically stored in R channel
		float texOcclusion = texture(occlusionTexture, fragTexCoord).r;

		/// Blend between base occlusion and texture value based on strength
		occlusionValue = mix(material.ambient, texOcclusion, material.occlusionStrength);
	}

	vec3 finalColor = vec3(0.0);

	/// Get normalized view direction for specular calculations
	vec3 viewDir = normalize(fragViewDir);

	/// Accumulate lighting from all active lights
	/// We add contributions from each light to create the final lighting
	for (int i = 0; i < 16; ++i) {  /// MaxLights from C++ code
		/// Only process lights with non-zero intensity
		if (lightData.lights[i].colorAndIntensity.a > 0.0) {
			finalColor += calculateDirectionalLight(
			lightData.lights[i],
			normal,
			albedo,
			viewDir,
			roughnessValue,
			metallicValue
			);
		}
	}

	/// Apply metallic and roughness factors
	finalColor = mix(finalColor, finalColor * metallicValue, metallicValue);
	finalColor = mix(finalColor, finalColor * roughnessValue, roughnessValue);

	/// Apply ambient occlusion
	finalColor *= occlusionValue;

	/// Apply a simple tone mapping to prevent over-saturation
	finalColor = finalColor / (finalColor + vec3(1.0));

	/// Output final color with material alpha
	float alpha = material.baseColor.a;
	if (material.useAlbedoTexture > 0.5) {
		/// If using texture, blend material alpha with texture alpha
		alpha *= texture(albedoTexture, fragTexCoord).a;
	}

	outColor = vec4(finalColor, alpha);
}