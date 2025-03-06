#version 450

/// Input from vertex shader
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragPosition;
layout(location = 3) in vec2 fragTexCoord;  /// Texture coordinates from vertex shader

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
} material;

/// Material textures (set = 2)
/// Separate bindings for different texture types
layout(set = 2, binding = 1) uniform sampler2D albedoTexture;  /// Base color/albedo texture

/// Calculate contribution from a single directional light
/// We separate this calculation to make the lighting logic clearer
/// and to allow for easy modification of the lighting model
vec3 calculateDirectionalLight(Light light, vec3 normal, vec3 albedo) {
	vec3 lightDir = -normalize(light.direction.xyz);
	vec3 lightColor = light.colorAndIntensity.rgb;
	float lightIntensity = light.colorAndIntensity.a;

	/// Calculate diffuse contribution
	float diff = max(dot(normal, lightDir), 0.0);
	vec3 diffuse = diff * lightColor * lightIntensity;

	/// Add ambient contribution
	vec3 ambient = light.ambient.rgb;

	/// Combine lighting with albedo and material properties
	/// This is a simplified PBR calculation - could be expanded
	return albedo * (ambient + diffuse);
}

void main() {
	vec3 normal = normalize(fragNormal);

	/// Sample albedo texture if enabled, otherwise use the base color
	/// This provides a smooth fallback when textures aren't available
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

	vec3 finalColor = vec3(0.0);

	/// Accumulate lighting from all active lights
	/// We add contributions from each light to create the final lighting
	for (int i = 0; i < 16; ++i) {  /// MaxLights from C++ code
		/// Only process lights with non-zero intensity
		/// This optimization skips calculations for disabled lights
		if (lightData.lights[i].colorAndIntensity.a > 0.0) {
			finalColor += calculateDirectionalLight(
			lightData.lights[i],
			normal,
			albedo
			);
		}
	}

	/// Apply metallic and roughness factors
	/// These parameters influence the final appearance:
	/// - Metallic affects reflection properties
	/// - Roughness affects surface scattering
	finalColor = mix(finalColor, finalColor * material.metallic, material.metallic);
	finalColor = mix(finalColor, finalColor * material.roughness, material.roughness);

	/// Apply ambient occlusion
	finalColor *= material.ambient;

	/// Apply a simple tone mapping to prevent over-saturation
	/// This becomes important when combining multiple lights
	finalColor = finalColor / (finalColor + vec3(1.0));

	/// Output final color with material alpha
	/// For textured materials, we could use the texture's alpha channel instead
	float alpha = material.baseColor.a;
	if (material.useAlbedoTexture > 0.5) {
		/// If using texture, blend material alpha with texture alpha
		alpha *= texture(albedoTexture, fragTexCoord).a;
	}

	outColor = vec4(finalColor, alpha);
}