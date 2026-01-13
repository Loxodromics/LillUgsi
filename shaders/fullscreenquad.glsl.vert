#version 450

/// Fullscreen Quad Vertex Shader
///
/// This shader demonstrates:
/// - Minimal vertex processing for fullscreen effects
/// - Pass-through of NDC positions (no transformation needed)
/// - UV coordinate forwarding for texture sampling

/// Vertex inputs matching QuadVertex structure
layout(location = 0) in vec2 inPosition;   /// NDC coordinates (-1 to 1)
layout(location = 1) in vec2 inTexCoord;   /// UV coordinates (0 to 1)

/// Fragment shader inputs
layout(location = 0) out vec2 fragTexCoord;

void main() {
	/// Position is already in NDC space (-1 to 1), no transformation needed
	/// Just add z=0 (middle of depth range) and w=1 (no perspective)
	gl_Position = vec4(inPosition, 0.0, 1.0);

	/// Pass texture coordinates to fragment shader
	fragTexCoord = inTexCoord;
}
