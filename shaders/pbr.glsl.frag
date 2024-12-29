#version 450

/// Input from vertex shader
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragPosition;

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
	float padding;         /// Required for std140 layout
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

	/// Combine lighting with base color and material properties
	/// This is a simplified PBR calculation - could be expanded
	return baseColor * material.baseColor.rgb * (ambient + diffuse);
}

void main() {
	vec3 normal = normalize(fragNormal);
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
			fragColor
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
	outColor = vec4(finalColor, material.baseColor.a);
}