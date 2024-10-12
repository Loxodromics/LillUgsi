#version 450

/// Input vertex attributes
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

/// Output to fragment shader
layout(location = 0) out vec3 fragColor;

/// Uniform buffer object containing view and projection matrices
layout(binding = 0) uniform UniformBufferObject {
	mat4 view;
	mat4 proj;
} ubo;

void main() {
	/// Calculate the final position in clip space
	/// We multiply the position by the view and projection matrices
	/// This transforms the vertex from world space to clip space
	gl_Position = ubo.proj * ubo.view * vec4(inPosition, 1.0);

	/// Pass the color to the fragment shader
	/// In a more complex shader, you might calculate lighting here
	fragColor = inColor;

	/// Optionally, we can reverse the depth value
	/// This can improve depth precision in some cases
//	gl_Position.z = (gl_Position.z + gl_Position.w) / 2.0;

	/// visualize the depthbuffer
//	fragColor = vec3(gl_Position.z, gl_Position.z, gl_Position.z);
}