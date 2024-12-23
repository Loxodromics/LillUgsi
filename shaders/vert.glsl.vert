#version 450

/// Input vertex attributes
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

/// Output to fragment shader
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;  /// World-space normal
layout(location = 2) out vec3 fragPosition;  /// World-space position

/// Uniform buffer object containing view and projection matrices
/// Camera transforms change less frequently than model transforms,
/// so we keep them in a uniform buffer
layout(binding = 0) uniform UniformBufferObject {
	mat4 view;
	mat4 proj;
} ubo;

/// Light data structure matches our C++ LightData struct
struct Light {
	vec4 direction;         /// Direction vector (w unused)
	vec4 colorAndIntensity; /// RGB color and intensity in w
	vec4 ambient;          /// Ambient color (w unused)
};

/// Single uniform buffer containing all lights
layout(binding = 1) uniform LightBuffer {
	Light lights[16];  /// Array size matches LightManager::MaxLights
} lightData;

/// Push constant block for model matrix
/// We use push constants for the model matrix because:
/// 1. It changes frequently (per-object)
/// 2. Small size (single 4x4 matrix)
/// 3. Fastest way to update shader data
layout(push_constant) uniform PushConstants {
	mat4 model;  /// World transform matrix
} push;

void main() {
	/// Calculate world-space position
	vec4 worldPos = push.model * vec4(inPosition, 1.0);

	/// Pass world-space position to fragment shader
	fragPosition = worldPos.xyz;

	/// Transform vertex position to clip space
	gl_Position = ubo.proj * ubo.view * worldPos;

	/// Transform normal to world space
	/// We use the inverse transpose of the model matrix to handle non-uniform scaling
	mat3 normalMatrix = transpose(inverse(mat3(push.model)));
	fragNormal = normalize(normalMatrix * inNormal);

	/// Pass the vertex color to fragment shader
	fragColor = inColor;

	/// For Reverse-Z, we invert the Z component
	/// This provides better depth precision
	gl_Position.z = (gl_Position.z + gl_Position.w) / 2.0;
}