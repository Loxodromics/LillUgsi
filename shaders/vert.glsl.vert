#version 450

/// Input vertex attributes
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

/// Output to fragment shader
layout(location = 0) out vec3 fragColor;

/// Uniform buffer object containing view and projection matrices
/// Camera transforms change less frequently than model transforms,
/// so we keep them in a uniform buffer
layout(binding = 0) uniform UniformBufferObject {
	mat4 view;
	mat4 proj;
} ubo;

/// Push constant block for model matrix
/// We use push constants for the model matrix because:
/// 1. It changes frequently (per-object)
/// 2. Small size (single 4x4 matrix)
/// 3. Fastest way to update shader data
layout(push_constant) uniform PushConstants {
	mat4 model;  /// World transform matrix
} push;

void main() {
	/// Transform the vertex position to clip space
	/// The transformation order is:
	/// 1. Model space to world space (model matrix)
	/// 2. World space to view space (view matrix)
	/// 3. View space to clip space (projection matrix)
	gl_Position = ubo.proj * ubo.view * push.model * vec4(inPosition, 1.0);

	/// Pass the color to the fragment shader
	/// In a more complex shader, we might calculate lighting here
	fragColor = inColor;

	/// For Reverse-Z, we invert the Z component
	/// This provides better depth precision
	gl_Position.z = (gl_Position.z + gl_Position.w) / 2.0;
}