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
	/// Calculate world-space position by transforming vertex position with model matrix
	/// We need the world position for lighting calculations in the fragment shader
	/// and to calculate the view direction from the camera to this fragment
	fragPosition = vec3(push.model * vec4(inPosition, 1.0));

	/// Calculate world-space normal by applying the normal matrix to the input normal
	/// We use the transpose of the inverse of the model matrix for correct normal transformation
	/// This ensures normals remain perpendicular to surfaces even with non-uniform scaling
	mat3 normalMatrix = transpose(inverse(mat3(push.model)));
	fragNormal = normalize(normalMatrix * inNormal);

	/// Pass vertex color directly to fragment shader
	/// This serves as a fallback color when textures aren't available
	/// and can also be used for vertex painting techniques
	fragColor = inColor;

	/// Pass texture coordinates to fragment shader for texture sampling
	/// We don't apply any transformations at this stage, as tiling is handled
	/// in the fragment shader based on per-texture settings
	fragTexCoord = inTexCoord;

	/// Calculate view direction from camera to fragment in world space
	/// This is needed for specular reflection calculations in the PBR BRDF
	/// We normalize in the vertex shader to save per-pixel normalization in the fragment shader
	fragViewDir = normalize(camera.cameraPos - fragPosition);

	/// Calculate and construct TBN matrix for normal mapping
	/// The TBN matrix transforms normal vectors from tangent space (normal map) to world space
	/// This is crucial for normal mapping to correctly orient detailed normals

	/// Transform the tangent to world space using the same normal matrix
	/// Since tangent is a direction vector like normal, it needs the same transformation
	vec3 T = normalize(normalMatrix * inTangent);

	/// Re-orthogonalize tangent with respect to normal using Gram-Schmidt process
	/// This step is crucial because even if tangents and normals are perpendicular in model space,
	/// the non-uniform scaling in the model matrix might make them non-perpendicular in world space
	/// Without this correction, normal mapping can produce distorted results
	T = normalize(T - dot(T, fragNormal) * fragNormal);

	/// Calculate bitangent using cross product of normal and tangent
	/// We use cross product to ensure the bitangent is perpendicular to both normal and tangent
	/// This creates a proper orthonormal basis for the tangent space to world space transformation
	vec3 B = normalize(cross(fragNormal, T));

	/// Construct the TBN matrix with the three orthonormal basis vectors
	/// Each column of the matrix represents one basis vector of the tangent space
	/// This matrix will transform vectors from tangent space to world space
	///
	/// Note that we use the columns in T, B, N order which is the convention for
	/// tangent space to world space transformation (maps x->T, y->B, z->N)
	fragTBN = mat3(T, B, fragNormal);

	/// Final required transformation: vertex position to clip space
	/// This is the position the GPU uses for rasterization and depth testing
	gl_Position = camera.proj * camera.view * vec4(fragPosition, 1.0);

	/// For Reverse-Z, we invert the Z component
	/// This provides better depth precision
	gl_Position.z = (gl_Position.z + gl_Position.w) / 2.0;
}