#version 450

/// Input from vertex shader
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragPosition;

/// Output color
layout(location = 0) out vec4 outColor;

/// Light data structure (must match vertex shader)
struct Light {
	vec4 direction;         /// Direction vector (w unused)
	vec4 colorAndIntensity; /// RGB color and intensity in w
	vec4 ambient;          /// Ambient color (w unused)
};

/// Single uniform buffer containing all lights
layout(binding = 1) uniform LightBuffer {
	Light lights[16];  /// Array size matches LightManager::MaxLights
} lightData;

void main() {
	/// Initialize lighting calculations
	vec3 normal = normalize(fragNormal);
	vec3 finalColor = vec3(0.0);

	/// Access first light using array syntax
	Light light = lightData.lights[0];

	vec3 lightDir = -normalize(light.direction.xyz);
	vec3 lightColor = light.colorAndIntensity.rgb;
	float lightIntensity = light.colorAndIntensity.a;

	/// Calculate diffuse lighting
	float diff = max(dot(normal, lightDir), 0.0);
	vec3 diffuse = diff * lightColor * lightIntensity;

	/// Add ambient light
	vec3 ambient = light.ambient.rgb;

	/// Combine lighting with vertex color
	finalColor = fragColor * (ambient + diffuse);

	/// Output final color
	outColor = vec4(finalColor, 1.0);
}