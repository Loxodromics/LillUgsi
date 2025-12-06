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
	uint lightCount;   /// Number of active lights
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

/// GGX Normal Distribution Function (Trowbridge-Reitz)
/// Determines the distribution of microfacet normals
/// NoH: dot(normal, halfVector)
/// roughness: surface roughness parameter [0,1]
float distributionGGX(float NoH, float roughness) {
	float a = roughness * roughness;
	float a2 = a * a;
	float NoH2 = NoH * NoH;
	float denom = (NoH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;
	return a2 / denom;
}

/// Schlick-GGX Geometry Function (single direction)
/// Models self-shadowing and masking of microfacets
/// NdotV: dot product between normal and view/light direction
/// roughness: surface roughness parameter [0,1]
float geometrySchlickGGX(float NdotV, float roughness) {
	float r = (roughness + 1.0);
	float k = (r * r) / 8.0;  /// Direct lighting formulation
	float denom = NdotV * (1.0 - k) + k;
	return NdotV / denom;
}

/// Smith's Method - Geometry Function
/// Combines view and light direction geometry attenuation
/// Accounts for both viewing and lighting geometry obstruction
/// NoV: dot(normal, viewDir)
/// NoL: dot(normal, lightDir)
/// roughness: surface roughness parameter [0,1]
float geometrySmith(float NoV, float NoL, float roughness) {
	float ggx1 = geometrySchlickGGX(NoV, roughness);  /// View direction
	float ggx2 = geometrySchlickGGX(NoL, roughness);  /// Light direction
	return ggx1 * ggx2;
}

/// Fresnel-Schlick Approximation
/// Calculates view-dependent reflectivity (increases at grazing angles)
/// cosTheta: dot(halfVector, viewDir) or dot(normal, viewDir) depending on use
/// F0: base reflectivity at normal incidence (0.04 for dielectrics, albedo for metals)
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

/// Extract Single Channel from Texture Sample
/// Useful for packed textures (e.g., ORM = Occlusion+Roughness+Metallic)
/// texSample: sampled texture value (vec4)
/// channelIndex: 0=R, 1=G, 2=B, 3=A
float extractChannel(vec4 texSample, uint channelIndex) {
	switch (channelIndex) {
		case 0: return texSample.r;
		case 1: return texSample.g;
		case 2: return texSample.b;
		case 3: return texSample.a;
		default: return texSample.r;  /// Fallback to red channel
	}
}

void main() {
	/// Renormalize TBN basis vectors after rasterizer interpolation
	/// Interpolating unit vectors does NOT preserve unit length!
	/// This is critical for correct normal mapping
	mat3 TBN = mat3(
		normalize(fragTBN[0]),  /// T (tangent)
		normalize(fragTBN[1]),  /// B (bitangent)
		normalize(fragTBN[2])   /// N (normal)
	);

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

	/// Process normal mapping
	/// Normal maps add fine surface detail without requiring additional geometry
	/// They store perturbed normal vectors in tangent space (RGB -> XYZ)
	vec3 normal;

	if (material.useNormalMap > 0.5) {
		/// Sample the normal map with tiling applied
		/// We use a separate tiling factor for normal maps to allow different
		/// detail scales for color and surface perturbation
		vec2 normalTexCoord = fragTexCoord * material.normalTiling;
		vec3 normalSample = texture(normalTexture, normalTexCoord).rgb;

		/// Transform normal from [0,1] range to [-1,1] range
		/// Normal maps typically store normals as RGB colors (0 to 1)
		/// but normals need to be in the -1 to 1 range for calculations
		vec3 tangentNormal = normalSample * 2.0 - 1.0;

		/// Apply normal strength factor to control the impact of the normal map
		/// When strength is 0, the normal remains pointing straight up in tangent space (0,0,1)
		/// When strength is 1, we use the full value from the normal map
		/// This lets artists control how pronounced the normal mapping effect is
		if (material.normalStrength < 1.0) {
			/// When normal strength is less than 1, we blend between the default
			/// tangent space normal (0,0,1) and the sampled normal
			/// Only the X and Y components are affected by strength to preserve the vector length
			tangentNormal.xy *= material.normalStrength;
			/// Re-normalize after scaling to ensure unit length
			tangentNormal = normalize(tangentNormal);
		}

		/// Transform normal from tangent space to world space using the TBN matrix
		/// This aligns the perturbed normal with the correct world orientation based on
		/// the surface geometry and texture coordinates
		normal = normalize(TBN * tangentNormal);
	} else {
		/// If no normal map is used, just use the renormalized surface normal from TBN
		/// This provides basic lighting without the added surface detail
		normal = TBN[2];  /// Already normalized when TBN was constructed
	}

	/// Sample roughness from texture or use uniform value
	/// Roughness controls the size of specular highlights (smooth vs rough surfaces)
	float roughness;
	if (material.useRoughnessMap > 0.5) {
		/// Apply tiling to texture coordinates for roughness map
		vec2 roughnessTexCoord = fragTexCoord * material.roughnessTiling;

		/// Sample roughness texture and extract the appropriate channel
		/// Many roughness maps are single-channel (grayscale) stored in R, G, or B
		vec4 roughnessSample = texture(roughnessTexture, roughnessTexCoord);
		roughness = extractChannel(roughnessSample, material.roughnessChannel);

		/// Apply roughness strength factor to control the influence of the texture
		/// When strength is 0, we use the base material.roughness value
		/// When strength is 1, we use the full texture value
		roughness = mix(material.roughness, roughness, material.roughnessStrength);
	} else {
		/// If no roughness map is enabled, use the uniform material value
		roughness = material.roughness;
	}

	/// Sample metallic from texture or use uniform value
	/// Metallic determines if a surface is metal (1.0) or dielectric (0.0)
	float metallic;
	if (material.useMetallicMap > 0.5) {
		/// Apply tiling to texture coordinates for metallic map
		vec2 metallicTexCoord = fragTexCoord * material.metallicTiling;

		/// Sample metallic texture and extract the appropriate channel
		/// Metallic maps are typically single-channel, often packed with roughness
		vec4 metallicSample = texture(metallicTexture, metallicTexCoord);
		metallic = extractChannel(metallicSample, material.metallicChannel);

		/// Apply metallic strength factor
		/// Allows artists to modulate the texture values
		metallic = mix(material.metallic, metallic, material.metallicStrength);
	} else {
		/// If no metallic map is enabled, use the uniform material value
		metallic = material.metallic;
	}

	/// Sample ambient occlusion from texture or use uniform value
	/// AO defines how much ambient light reaches different parts of the surface
	/// Crevices and occluded areas typically have lower values (darker)
	float occlusion;
	if (material.useOcclusionMap > 0.5) {
		/// Apply tiling to texture coordinates for occlusion map
		vec2 occlusionTexCoord = fragTexCoord * material.occlusionTiling;

		/// Sample occlusion texture and extract the appropriate channel
		/// Occlusion maps are typically single-channel (R) or packed in ORM textures
		vec4 occlusionSample = texture(occlusionTexture, occlusionTexCoord);
		occlusion = extractChannel(occlusionSample, material.occlusionChannel);

		/// Apply occlusion strength factor to control the influence of the texture
		/// When strength is 0, we use the base material.ambient value
		/// When strength is 1, we use the full texture value
		occlusion = mix(material.ambient, occlusion, material.occlusionStrength);
	} else {
		/// If no occlusion map is enabled, use the uniform material value
		occlusion = material.ambient;
	}

	/// Renormalize view direction after rasterizer interpolation
	/// Interpolating unit vectors does NOT preserve unit length
	/// This is critical for accurate specular calculations
	vec3 viewDir = normalize(fragViewDir);

	/// Initialize the final color with the ambient term
	/// This represents indirect light from the environment
	/// Even shadowed areas receive this minimal lighting
	vec3 finalColor = vec3(0.0);
	vec3 ambientColor = vec3(0.0);

	/// Process active lights
	/// Using dynamic light count avoids iterating over inactive lights
	/// This improves performance when fewer than the maximum lights are active
	for (int i = 0; i < int(lightData.lightCount); i++) {
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

		/// Calculate half-vector between view and light directions
		/// Used for specular reflection calculations
		vec3 H = normalize(viewDir + lightDir);

		/// Calculate dot products needed for BRDF terms
		float NoL = max(dot(normal, lightDir), 0.0);     /// Lambert term (also used for diffuse)
		float NoV = max(dot(normal, viewDir), 0.0);  /// View angle
		float NoH = max(dot(normal, H), 0.0);            /// Half-vector angle
		float VoH = max(dot(viewDir, H), 0.0);       /// View-half angle

		/// Calculate F0 (base reflectivity) based on sampled metallic value
		/// Now uses texture-driven metallic for spatially-varying metal/dielectric behavior
		/// Dielectrics (metallic=0): F0 = 0.04 (4% reflectance, white specular)
		/// Metals (metallic=1): F0 = albedo (colored specular from base color)
		vec3 F0 = mix(vec3(0.04), albedo, metallic);

		/// Calculate diffuse term using Lambert's cosine law
		/// The division by PI normalizes the Lambert BRDF to ensure energy conservation
		vec3 diffuse = albedo / PI * NoL;

		/// Cook-Torrance Specular BRDF with sampled roughness
		/// BRDF = (D * F * G) / (4 * NoV * NoL)
		/// where D = distribution, F = fresnel, G = geometry
		/// Roughness now varies across the surface based on the texture
		float D = distributionGGX(NoH, roughness);
		vec3 F = fresnelSchlick(VoH, F0);
		float G = geometrySmith(NoV, NoL, roughness);

		/// Calculate kD (diffuse coefficient) for energy conservation
		/// kD represents the fraction of light that is refracted (diffuse) rather than reflected (specular)
		/// - (1.0 - F): Light not reflected is refracted (diffuse)
		/// - (1.0 - metallic): Metals have no diffuse component (kD = 0 when metallic = 1)
		/// This ensures energy conservation: diffuse + specular <= 1.0
		/// kD now varies spatially with the metallic texture
		vec3 kD = (1.0 - F) * (1.0 - metallic);

		/// Combine terms (prevent division by zero with epsilon)
		vec3 numerator = D * F * G;
		float denominator = 4.0 * NoV * NoL + EPSILON;
		vec3 specular = numerator / denominator;

		/// Apply energy conservation
		/// Multiply diffuse by kD to ensure total energy (diffuse + specular) <= 1.0
		/// When F is high (grazing angles or metals), kD is low (less diffuse)
		/// When metallic = 1.0, kD = 0 (no diffuse contribution for pure metals)
		finalColor += (kD * diffuse + specular) * lightColor;
	}

	/// Add accumulated ambient light with occlusion
	/// Occlusion now varies spatially based on the texture
	/// Areas with low occlusion (dark AO map) receive less ambient light
	/// This creates realistic shadowing in crevices, corners, and contact points
	finalColor += ambientColor * albedo * occlusion;

	/// Simple tone mapping (Reinhard operator)
	/// This compresses HDR values into LDR range for display
	/// We'll implement more sophisticated tone mapping in later stages
	finalColor = finalColor / (finalColor + vec3(1.0));

	/// Set output color with opaque alpha
	/// We use the alpha channel from the material's base color
	/// This preserves any transparency settings set by the artist
	outColor = vec4(finalColor, material.baseColor.a);
}