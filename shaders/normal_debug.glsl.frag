#version 450

/// Debug shader for validating normal mapping implementation
/// This shader provides various visualization modes to verify correctness

/// Input from vertex shader
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragPosition;
layout(location = 3) in vec2 fragTexCoord;
layout(location = 4) in mat3 fragTBN;
layout(location = 7) in vec3 fragViewDir;

/// Output color
layout(location = 0) out vec4 outColor;

/// Camera for additional calculations
layout(set = 0, binding = 0) uniform CameraUBO {
	mat4 view;
	mat4 proj;
	vec3 cameraPos;
} camera;

/// Material uniforms
layout(set = 2, binding = 0) uniform MaterialUBO {
	vec4 baseColor;
	float roughness;
	float metallic;
	float ambient;
	float useAlbedoTexture;
	float useNormalMap;
	float useRoughnessMap;
	float useMetallicMap;
	float useOcclusionMap;
	float normalStrength;
	float roughnessStrength;
	float metallicStrength;
	float occlusionStrength;
	vec2 albedoTiling;
	vec2 normalTiling;
	vec2 roughnessTiling;
	vec2 metallicTiling;
	vec2 occlusionTiling;
	uint roughnessChannel;
	uint metallicChannel;
	uint occlusionChannel;
	uint debugMode;  /// Debug visualization mode
} material;

/// Textures
layout(set = 2, binding = 1) uniform sampler2D albedoTexture;
layout(set = 2, binding = 2) uniform sampler2D normalTexture;

const float PI = 3.14159265359;

/// Convert normal from [-1,1] to [0,1] for visualization
vec3 normalToColor(vec3 normal) {
	return normal * 0.5 + 0.5;
}

/// Extract individual TBN components
vec3 getTangent() {
	return fragTBN[0];
}

vec3 getBitangent() {
	return fragTBN[1];
}

vec3 getNormal() {
	return fragTBN[2];
}

/// Check if a vector is normalized (length ~1)
bool isNormalized(vec3 v) {
	float len = length(v);
	return abs(len - 1.0) < 0.01;
}

/// Check if two vectors are orthogonal (dot ~0)
bool isOrthogonal(vec3 a, vec3 b) {
	float d = dot(a, b);
	return abs(d) < 0.1;
}

/// Visualize vector length as color
/// Green = correct (length 1)
/// Red = too long
/// Blue = too short
vec3 lengthToColor(vec3 v) {
	float len = length(v);
	if (abs(len - 1.0) < 0.01) {
		return vec3(0, 1, 0);  /// Green for correct
	} else if (len > 1.0) {
		return vec3(1, 0, 0);  /// Red for too long
	} else {
		return vec3(0, 0, 1);  /// Blue for too short
	}
}

/// Visualize orthogonality between two vectors
/// Green = orthogonal (dot ~0)
/// Red = parallel (dot ~1)
/// Yellow = somewhere in between
vec3 orthogonalityToColor(vec3 a, vec3 b) {
	float d = abs(dot(normalize(a), normalize(b)));
	if (d < 0.1) {
		return vec3(0, 1, 0);  /// Green for orthogonal
	} else if (d > 0.9) {
		return vec3(1, 0, 0);  /// Red for parallel
	} else {
		return vec3(1, 1, 0);  /// Yellow for in-between
	}
}

void main() {
	/// Extract TBN components and RENORMALIZE after rasterizer interpolation
	/// Interpolating unit vectors does NOT preserve unit length!
	vec3 T = normalize(getTangent());
	vec3 B = normalize(getBitangent());
	vec3 N = normalize(getNormal());

	/// Get the mapped normal (if normal mapping is enabled)
	vec3 mappedNormal = N;
	vec3 tangentNormal = vec3(0.0);

	if (material.useNormalMap > 0.5) {
		vec2 normalTexCoord = fragTexCoord * material.normalTiling;
		vec3 normalSample = texture(normalTexture, normalTexCoord).rgb;
		tangentNormal = normalSample * 2.0 - 1.0;

		if (material.normalStrength < 1.0) {
			tangentNormal.xy *= material.normalStrength;
			tangentNormal = normalize(tangentNormal);
		}

		mappedNormal = normalize(fragTBN * tangentNormal);
	}

	/// Simple directional light for reference
	vec3 lightDir = normalize(vec3(0.3, -1.0, 0.5));
	float lightDot = max(dot(N, -lightDir), 0.0);
	float mappedLightDot = max(dot(mappedNormal, -lightDir), 0.0);

	/// Debug mode visualization
	switch (material.debugMode) {
		/// Mode 0: Normal rendering (basic Lambert with normal map)
		case 0u: {
			vec3 albedo = material.baseColor.rgb;
			if (material.useAlbedoTexture > 0.5) {
				albedo = texture(albedoTexture, fragTexCoord * material.albedoTiling).rgb;
			}
			vec3 lighting = albedo * mappedLightDot;
			outColor = vec4(lighting, 1.0);
			break;
		}

		/// Mode 1: Vertex normals (world space)
		/// Should show smooth color gradients
		/// Sphere: radial gradient from all sides
		/// Cube: solid colors per face
		case 1u: {
			outColor = vec4(normalToColor(normalize(fragNormal)), 1.0);
			break;
		}

		/// Mode 2: TBN Tangent (world space)
		/// Should follow UV U direction
		case 2u: {
			outColor = vec4(normalToColor(T), 1.0);
			break;
		}

		/// Mode 3: TBN Bitangent (world space)
		/// Should follow UV V direction
		case 3u: {
			outColor = vec4(normalToColor(B), 1.0);
			break;
		}

		/// Mode 4: TBN Normal (world space)
		/// Should match mode 1 for smooth surfaces
		case 4u: {
			outColor = vec4(normalToColor(N), 1.0);
			break;
		}

		/// Mode 5: Raw normal map sample (tangent space, as RGB texture)
		/// Should be mostly blue/purple (Z dominant in tangent space)
		/// Flat areas should be pure blue (0.5, 0.5, 1.0) → (128, 128, 255)
		case 5u: {
			if (material.useNormalMap > 0.5) {
				vec2 normalTexCoord = fragTexCoord * material.normalTiling;
				vec3 rawSample = texture(normalTexture, normalTexCoord).rgb;
				outColor = vec4(rawSample, 1.0);
			} else {
				outColor = vec4(0.5, 0.5, 1.0, 1.0);  /// Default tangent space normal
			}
			break;
		}

		/// Mode 6: Normal map in tangent space (after [-1,1] conversion)
		/// Should show perturbations around (0,0,1) → displayed as blue
		case 6u: {
			if (material.useNormalMap > 0.5) {
				outColor = vec4(normalToColor(tangentNormal), 1.0);
			} else {
				outColor = vec4(0.5, 0.5, 1.0, 1.0);
			}
			break;
		}

		/// Mode 7: Final world-space normal (after TBN transform)
		/// Should show surface detail from normal map
		case 7u: {
			outColor = vec4(normalToColor(mappedNormal), 1.0);
			break;
		}

		/// Mode 8: UV coordinates
		/// R = U, G = V, B = 0
		/// Should show smooth red→green gradient based on UVs
		case 8u: {
			outColor = vec4(fragTexCoord.x, fragTexCoord.y, 0.0, 1.0);
			break;
		}

		/// Mode 9: Simple lighting (no normal map)
		/// Shows how vertex normals respond to light
		case 9u: {
			outColor = vec4(vec3(lightDot), 1.0);
			break;
		}

		/// Mode 10: Simple lighting (with normal map)
		/// Should show additional detail compared to mode 9
		case 10u: {
			outColor = vec4(vec3(mappedLightDot), 1.0);
			break;
		}

		/// Mode 11: Difference between vertex normal and mapped normal
		/// Shows how much normal map perturbs the surface
		/// Should be mostly dark (small perturbations)
		case 11u: {
			vec3 diff = abs(mappedNormal - N);
			outColor = vec4(diff * 5.0, 1.0);  /// Amplify for visibility
			break;
		}

		/// Mode 12: TBN orthogonality check
		/// Green = all vectors orthogonal (correct)
		/// Red = vectors not orthogonal (problem!)
		case 12u: {
			vec3 tbn_check = vec3(0.0);

			/// Check T ⊥ N
			if (isOrthogonal(T, N)) {
				tbn_check.r = 1.0;
			}

			/// Check B ⊥ N
			if (isOrthogonal(B, N)) {
				tbn_check.g = 1.0;
			}

			/// Check T ⊥ B
			if (isOrthogonal(T, B)) {
				tbn_check.b = 1.0;
			}

			outColor = vec4(tbn_check, 1.0);
			/// Should be white (1,1,1) everywhere if TBN is orthonormal
			break;
		}

		/// Mode 13: TBN length check
		/// Green = all vectors normalized (correct)
		/// Red/Blue = vectors not unit length (problem!)
		case 13u: {
			vec3 length_check = vec3(0.0);

			if (isNormalized(T)) length_check.r = 1.0;
			if (isNormalized(B)) length_check.g = 1.0;
			if (isNormalized(N)) length_check.b = 1.0;

			outColor = vec4(length_check, 1.0);
			/// Should be white (1,1,1) everywhere if TBN vectors are normalized
			break;
		}

		/// Mode 14: T·N dot product visualization
		/// Should be near zero (orthogonal)
		/// Green = good, Red = bad
		case 14u: {
			float d = abs(dot(T, N));
			vec3 color = mix(vec3(0,1,0), vec3(1,0,0), d * 10.0);
			outColor = vec4(color, 1.0);
			break;
		}

		/// Mode 15: B·N dot product visualization
		case 15u: {
			float d = abs(dot(B, N));
			vec3 color = mix(vec3(0,1,0), vec3(1,0,0), d * 10.0);
			outColor = vec4(color, 1.0);
			break;
		}

		/// Mode 16: T·B dot product visualization
		case 16u: {
			float d = abs(dot(T, B));
			vec3 color = mix(vec3(0,1,0), vec3(1,0,0), d * 10.0);
			outColor = vec4(color, 1.0);
			break;
		}

		/// Mode 17: Face direction check
		/// Green = facing camera, Red = facing away
		case 17u: {
			vec3 viewDir = normalize(camera.cameraPos - fragPosition);
			float facing = dot(N, viewDir);
			if (facing > 0.0) {
				outColor = vec4(0, 1, 0, 1.0);  /// Green = front facing
			} else {
				outColor = vec4(1, 0, 0, 1.0);  /// Red = back facing
			}
			break;
		}

		/// Mode 18: View-dependent rim lighting
		/// Should be bright at glancing angles, dark when facing camera
		case 18u: {
			vec3 viewDir = normalize(camera.cameraPos - fragPosition);
			float rim = 1.0 - abs(dot(N, viewDir));
			outColor = vec4(vec3(rim), 1.0);
			break;
		}

		/// Mode 19: Tangent space visualization
		/// R = tangent space X (tangent in world space)
		/// G = tangent space Y (bitangent in world space)
		/// B = tangent space Z (normal in world space)
		case 19u: {
			/// Project world normal into tangent space for visualization
			vec3 tsNormal = vec3(
				dot(mappedNormal, T),
				dot(mappedNormal, B),
				dot(mappedNormal, N)
			);
			outColor = vec4(normalToColor(tsNormal), 1.0);
			break;
		}

		/// Mode 20: Normal map strength visualization
		/// Shows effect of normal strength parameter
		case 20u: {
			if (material.useNormalMap > 0.5) {
				/// Compare full strength vs current strength
				vec3 fullStrength = normalize(fragTBN * tangentNormal);
				float diff = length(fullStrength - mappedNormal);
				outColor = vec4(vec3(diff * 5.0), 1.0);
			} else {
				outColor = vec4(0, 0, 0, 1.0);
			}
			break;
		}

		/// Default: Magenta for invalid mode
		default: {
			outColor = vec4(1, 0, 1, 1.0);
			break;
		}
	}
}
