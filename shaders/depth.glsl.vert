#version 450

/// Depth-only vertex shader for depth pre-pass
/// Transforms vertex positions with minimal processing
/// All vertex attributes are declared to match the vertex format,
/// but only position is actually used

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec3 inColor;
layout(location = 4) in vec2 inTexCoord;

/// Camera uniform buffer (set 0)
layout(set = 0, binding = 0) uniform CameraUBO {
	mat4 view;
	mat4 proj;
	vec3 cameraPos;
} camera;

/// Model matrix via push constants
layout(push_constant) uniform PushConstants {
	mat4 model;
} push;

void main() {
	/// Transform vertex to clip space
	/// With GLM_FORCE_DEPTH_ZERO_TO_ONE and swapped near/far planes,
	/// Reverse-Z is handled directly by the projection matrix
	vec4 worldPos = push.model * vec4(inPosition, 1.0);
	gl_Position = camera.proj * camera.view * worldPos;
}
