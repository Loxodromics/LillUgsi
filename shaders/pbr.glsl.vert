#version 450

/// Input vertex attributes
/// These attributes match the Vertex structure in C++
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;      /// Tangent vector for normal mapping
layout(location = 3) in vec3 inColor;
layout(location = 4) in vec2 inTexCoord;     /// Texture coordinate input

/// Output to fragment shader
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;    /// World-space normal
layout(location = 2) out vec3 fragPosition;  /// World-space position
layout(location = 3) out vec2 fragTexCoord;  /// Pass texture coordinates to fragment shader
layout(location = 4) out mat3 fragTBN;       /// TBN matrix for normal mapping (uses locations 4, 5, 6)
layout(location = 7) out vec3 fragViewDir;   /// View direction in world space

/// Camera uniform buffer (set = 0)
layout(set = 0, binding = 0) uniform CameraUBO {
	mat4 view;
	mat4 proj;
	vec3 cameraPos;        /// Added camera position for view direction calculation
} camera;

/// Light data structure matches our C++ LightData struct
struct Light {
	vec4 direction;         /// Direction vector (w unused)
	vec4 colorAndIntensity; /// RGB color and intensity in w
	vec4 ambient;           /// Ambient color (w unused)
};

/// Light buffer (set = 1)
/// Separate set allows for efficient light updates
layout(set = 1, binding = 0) uniform LightBuffer {
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

	/// Calculate view direction (from position to camera)
	/// Normalize for consistent lighting calculations
	fragViewDir = normalize(camera.cameraPos - worldPos.xyz);

	/// Transform vertex position to clip space
	gl_Position = camera.proj * camera.view * worldPos;

	/// Transform normal to world space
	/// We use the inverse transpose of the model matrix to handle non-uniform scaling
	mat3 normalMatrix = transpose(inverse(mat3(push.model)));
	vec3 worldNormal = normalize(normalMatrix * inNormal);
	fragNormal = worldNormal;

	/// Transform tangent to world space
	/// This uses the same normal matrix as for the normal vector
	vec3 worldTangent = normalize(normalMatrix * inTangent);

	/// Re-orthogonalize tangent with respect to normal
	/// This ensures we have an orthogonal TBN basis
	worldTangent = normalize(worldTangent - worldNormal * dot(worldNormal, worldTangent));

	/// Calculate bitangent from normal and tangent
	/// Using the cross product ensures we have a proper orthonormal basis
	vec3 worldBitangent = cross(worldNormal, worldTangent);

	/// Build the TBN matrix for transforming normals from tangent space to world space
	/// Each column of the matrix is one of our basis vectors
	fragTBN = mat3(
	worldTangent,    // First column: tangent (X axis in tangent space)
	worldBitangent,  // Second column: bitangent (Y axis in tangent space)
	worldNormal      // Third column: normal (Z axis in tangent space)
	);

	/// Pass the vertex color to fragment shader
	fragColor = inColor;

	/// Pass texture coordinates to fragment shader
	fragTexCoord = inTexCoord;

	/// For Reverse-Z, we invert the Z component
	/// This provides better depth precision
	gl_Position.z = (gl_Position.z + gl_Position.w) / 2.0;
}