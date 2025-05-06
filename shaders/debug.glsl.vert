#version 450

/// Input vertex attributes
/// These attributes match the Vertex structure in C++
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec3 inColor;
layout(location = 4) in vec2 inTexCoord;

/// Output to fragment shader
/// We only need to pass the vertex color for debug visualization
layout(location = 0) out vec3 fragColor;

/// Camera uniform buffer (set = 0)
/// We need camera matrices for proper 3D positioning
layout(set = 0, binding = 0) uniform CameraUBO {
	mat4 view;
	mat4 proj;
	vec3 cameraPos;
} camera;

/// Push constant block for model matrix
/// We use push constants for the model matrix for efficient per-object updates
layout(push_constant) uniform PushConstants {
	mat4 model;
} push;

void main() {
	/// Calculate world-space position
	/// We transform from model space to world space using the model matrix
	vec4 worldPos = push.model * vec4(inPosition, 1.0);

	/// Transform vertex position to clip space
	/// This chain of transformations: model -> world -> view -> projection
	gl_Position = camera.proj * camera.view * worldPos;

	/// Pass the vertex color to fragment shader
	/// For debugging, we want to see the raw vertex colors
	fragColor = inColor;

	/// For Reverse-Z, we invert the Z component
	/// This provides better depth precision in our renderer
	gl_Position.z = (gl_Position.z + gl_Position.w) / 2.0;
}