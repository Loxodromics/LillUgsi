#version 450

/// Input vertex attributes that match our Vertex structure in C++
/// The vertex data is already displaced by PlanetGenerator - we just pass it through
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;    /// R channel contains height data (R8 format)

/// Output to fragment shader
layout(location = 0) out vec3 fragPosition;  /// World-space position for later use
layout(location = 1) out vec3 fragNormal;    /// World-space normal for lighting
layout(location = 2) out float fragHeight;   /// Normalized height for biome selection

/// Camera uniform buffer (set = 0)
/// We keep camera data in its own descriptor set for potential multi-view rendering
layout(set = 0, binding = 0) uniform CameraUBO {
	mat4 view;
	mat4 proj;
} camera;

/// Light data structure matches our C++ LightData struct
struct Light {
	vec4 direction;         /// Direction vector (w unused)
	vec4 colorAndIntensity; /// RGB color and intensity in w
	vec4 ambient;          /// Ambient color (w unused)
};

/// Light buffer (set = 1)
/// Separate set allows for efficient light updates
layout(set = 1, binding = 0) uniform LightBuffer {
	Light lights[16];       /// Array size matches LightManager::MaxLights
} lightData;

/// Push constant block for model matrix
/// We use push constants for the model matrix because:
/// 1. It changes frequently (per-object)
/// 2. Small size (single 4x4 matrix)
/// 3. Fastest way to update shader data
layout(push_constant) uniform PushConstants {
	mat4 model;            /// World transform matrix
} push;

void main() {
	/// Calculate world-space position
	vec4 worldPos = push.model * vec4(inPosition, 1.0);

	/// Pass world-space position to fragment shader
	fragPosition = worldPos.xyz;

	/// Transform normal to world space
	/// We use the inverse transpose of the model matrix to handle non-uniform scaling
	mat3 normalMatrix = transpose(inverse(mat3(push.model)));
	fragNormal = normalize(normalMatrix * inNormal);

	/// Extract height from vertex color's red channel
	/// For now, we use R8 format (0-255 mapped to 0-1)
	/// TODO: Later upgrade to R16_SFLOAT for better precision
	fragHeight = inColor.r;

	/// Transform vertex position to clip space
	gl_Position = camera.proj * camera.view * worldPos;

	/// For Reverse-Z, we invert the Z component
	/// This provides better depth precision
	gl_Position.z = (gl_Position.z + gl_Position.w) / 2.0;
}