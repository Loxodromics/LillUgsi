#version 450

/// Input vertex attributes
/// We keep the same vertex format as other materials for consistency
/// and to allow the wireframe material to work with any mesh
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

/// Camera uniform buffer (set = 0)
/// We share the same camera data structure across all materials
/// This ensures consistent view and projection transformations
layout(set = 0, binding = 0) uniform CameraUBO {
	mat4 view;
	mat4 proj;
} camera;

/// Push constants for model matrix
/// Using push constants for per-object transforms provides
/// the most efficient way to update frequently changing data
layout(push_constant) uniform PushConstants {
	mat4 model;  /// World transform matrix
} push;

void main() {
	/// Calculate world-space position
	/// We follow the same transform chain as other materials:
	/// 1. Model matrix transforms vertex to world space
	/// 2. View matrix transforms to camera space
	/// 3. Projection matrix transforms to clip space
	vec4 worldPos = push.model * vec4(inPosition, 1.0);
	gl_Position = camera.proj * camera.view * worldPos;

	/// For Reverse-Z, we invert the Z component
	/// This matches the depth buffer configuration in the renderer
	/// and provides better depth precision
	gl_Position.z = (gl_Position.z + gl_Position.w) / 2.0;
}