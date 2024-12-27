#version 450

/// Input from vertex shader
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragPosition;

/// Output color
layout(location = 0) out vec4 outColor;

/// Light data structure
struct Light {
	vec4 direction;         /// Direction vector (w unused)
	vec4 colorAndIntensity; /// RGB color and intensity in w
	vec4 ambient;          /// Ambient color (w unused)
};

/// Single uniform buffer containing all lights
layout(binding = 1) uniform LightBuffer {
	Light lights[16];  /// Array size matches LightManager::MaxLights
} lightData;

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

	/// Combine lighting with base color
	return baseColor * (ambient + diffuse);
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
			finalColor += calculateDirectionalLight(lightData.lights[i], normal, fragColor);
		}
	}

	/// Apply a simple tone mapping to prevent over-saturation
	/// This becomes important when combining multiple lights
	finalColor = finalColor / (finalColor + vec3(1.0));

	/// Output final color
	outColor = vec4(finalColor, 1.0);
}