#version 450

/// Input from vertex shader
layout(location = 0) in vec3 fragColor;

/// Output color
layout(location = 0) out vec4 outColor;

/// Debug material properties (set = 2)
/// We use a simple uniform buffer to allow runtime color adjustment
layout(set = 2, binding = 0) uniform DebugUBO {
	vec3 colorMultiplier;
	float padding;
} debug;

void main() {
	/// Apply color multiplier to vertex color
	/// This allows us to tint all vertices uniformly for testing
	vec3 finalColor = fragColor * debug.colorMultiplier;

	/// Output the final color with full opacity
	/// For debugging, we always want solid colors to clearly see faces
	outColor = vec4(finalColor, 1.0);
}