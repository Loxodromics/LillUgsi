#version 450

/// Input vertex attributes
/// These attributes match the Vertex structure in C++
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec3 inColor;
layout(location = 4) in vec2 inTexCoord;

/// Output to fragment shader
/// We pass vertex specific data for different visualization modes
layout(location = 0) out vec3 fragColor;      /// For vertex color mode
layout(location = 1) out vec3 fragNormal;     /// For normal visualization
layout(location = 2) out vec3 fragPosition;   /// For winding order visualization
layout(location = 3) out vec3 fragViewDir;    /// For view-dependent effects

/// Camera uniform buffer (set = 0)
/// We need camera matrices for proper 3D positioning
layout(set = 0, binding = 0) uniform CameraUBO {
	mat4 view;
	mat4 proj;
	vec3 cameraPos;
} camera;

/// Debug uniform buffer (set = 2)
/// Controls visualization mode and color adjustments
layout(set = 2, binding = 0) uniform DebugUBO {
	vec3 colorMultiplier;
	int visualizationMode;    /// 0=VertexColors, 1=NormalColors, 2=WindingOrder
} debug;

/// Push constant block for model matrix
/// We use push constants for the model matrix for efficient per-object updates
layout(push_constant) uniform PushConstants {
	mat4 model;
} push;

void main() {
	/// Calculate world-space position
	/// We transform from model space to world space using the model matrix
	vec4 worldPos = push.model * vec4(inPosition, 1.0);
	fragPosition = worldPos.xyz;

	/// Calculate view direction vector for view-dependent visualizations
	fragViewDir = normalize(camera.cameraPos - worldPos.xyz);

	/// Transform vertex position to clip space
	/// This chain of transformations: model -> world -> view -> projection
	gl_Position = camera.proj * camera.view * worldPos;

	/// Transform normal to world space for visualization
	/// We use the inverse transpose to handle non-uniform scaling correctly
	mat3 normalMatrix = transpose(inverse(mat3(push.model)));
	fragNormal = normalize(normalMatrix * inNormal);

	/// Pass vertex color - will be used in vertex color mode
	fragColor = inColor;

	/// For Reverse-Z, we invert the Z component
	/// This provides better depth precision in our renderer
	gl_Position.z = (gl_Position.z + gl_Position.w) / 2.0;
}