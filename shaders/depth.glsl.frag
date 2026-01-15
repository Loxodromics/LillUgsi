#version 450

/// Empty fragment shader for depth-only rendering
/// Vulkan requires a fragment shader even for depth-only pipelines
///
/// Early-Z test happens before fragment shader execution:
/// - Fragments that fail depth test never execute this shader
/// - Depth values are written automatically by the hardware
/// - This shader body is never actually executed for occluded fragments

void main() {
	/// No output needed - depth is written automatically
	/// The GPU's depth test hardware handles everything
}
