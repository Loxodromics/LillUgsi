#version 450

/// Output color
layout(location = 0) out vec4 outColor;

/// Material properties (set = 2)
/// We use a simple uniform buffer for wireframe color
/// This matches the Properties structure in WireframeMaterial
layout(set = 2, binding = 0) uniform MaterialUBO {
	vec3 color;    /// RGB color for wireframe lines
	float padding; /// Required for std140 layout
} material;

void main() {
	/// Output the wireframe color with full opacity
	/// We use alpha = 1.0 for solid lines, but the alpha blending
	/// is configured in the pipeline if transparency is needed
	outColor = vec4(material.color, 1.0);
}