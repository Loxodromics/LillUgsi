#version 450

/// Input vertex attributes
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec3 inColor;
layout(location = 4) in vec2 inTexCoord;

/// Output to fragment shader
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragPosition;
layout(location = 3) out vec2 fragTexCoord;
layout(location = 4) out mat3 fragTBN;
layout(location = 7) out vec3 fragViewDir;

/// Camera uniform buffer (set = 0)
layout(set = 0, binding = 0) uniform CameraUBO {
	mat4 view;
	mat4 proj;
	vec3 cameraPos;
} camera;

/// Push constant for model matrix
layout(push_constant) uniform PushConstants {
	mat4 model;
} push;

void main() {
	/// Calculate world-space position
	vec4 worldPos = push.model * vec4(inPosition, 1.0);
	fragPosition = worldPos.xyz;

	/// Calculate world-space normal
	mat3 normalMatrix = transpose(inverse(mat3(push.model)));
	fragNormal = normalize(normalMatrix * inNormal);

	/// Pass vertex color
	fragColor = inColor;

	/// Pass texture coordinates
	fragTexCoord = inTexCoord;

	/// Calculate view direction
	fragViewDir = normalize(camera.cameraPos - fragPosition);

	/// Calculate TBN matrix for normal mapping
	vec3 T = normalize(normalMatrix * inTangent);
	T = normalize(T - dot(T, fragNormal) * fragNormal);
	vec3 B = normalize(cross(fragNormal, T));
	fragTBN = mat3(T, B, fragNormal);

	/// Transform to clip space
	gl_Position = camera.proj * camera.view * worldPos;

	/// Apply Reverse-Z depth transformation
	gl_Position.z = (gl_Position.z + gl_Position.w) / 2.0;
}
