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

/// Calculate contribution from a single directional light with specular component
/// Now using view direction for proper specular reflection
vec3 calculateDirectionalLight(Light light, vec3 normal, vec3 albedo, vec3 viewDir, float roughness, float metallic) {
	vec3 lightDir = -normalize(light.direction.xyz);
	vec3 lightColor = light.colorAndIntensity.rgb;
	float lightIntensity = light.colorAndIntensity.a;

	/// Calculate diffuse contribution
	float diff = max(dot(normal, lightDir), 0.0);
	vec3 diffuse = diff * lightColor * lightIntensity;

	/// Calculate half vector for specular
	vec3 halfVec = normalize(lightDir + viewDir);

	/// Calculate specular component using the Blinn-Phong model
	/// For roughness, a higher value means less specular focus
	float specularPower = (1.0 - roughness) * 128.0 + 1.0;
	float spec = pow(max(dot(normal, halfVec), 0.0), specularPower);

	/// Adjust specular intensity based on metallic property
	/// Metallic surfaces have stronger reflections
	float specularIntensity = mix(0.04, 1.0, metallic);
	vec3 specular = spec * specularIntensity * lightColor * lightIntensity;

	/// Add ambient contribution
	vec3 ambient = light.ambient.rgb;

	/// For metallic surfaces, tint the specular with the albedo color
	/// This simulates how metals color their reflections
	if (metallic > 0.0) {
		specular *= mix(vec3(1.0), albedo, metallic);
	}

	/// Combine lighting components
	/// Non-metals have white specular, while metals have colored specular
	return albedo * (ambient + diffuse) + specular;
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