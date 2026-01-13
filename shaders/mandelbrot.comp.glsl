#version 450

/// Mandelbrot Fractal Generator - Compute Shader
///
/// This shader demonstrates:
/// - imageStore() for writing to storage images
/// - 2D compute dispatch (16x16 work groups)
/// - Push constants for interactive parameters
/// - Complex number mathematics for fractal generation

/// Local work group size: 16x16 = 256 threads per work group
layout(local_size_x = 16, local_size_y = 16) in;

/// Storage image output (matches descriptor set from Step 4)
/// - rgba8 format matches VK_FORMAT_R8G8B8A8_UNORM
/// - writeonly: we only write, never read
/// - set=0, binding=0: matches our descriptor layout
layout(rgba8, set = 0, binding = 0) uniform writeonly image2D outputImage;

/// Push constants for runtime parameters (Step 6 will use these)
/// These allow interactive control without rebuilding the pipeline
layout(push_constant) uniform PushConstants {
	vec2 offset;      /// Center point in complex plane
	float zoom;       /// Scale factor (smaller = more zoomed in)
	int maxIter;      /// Maximum iteration count
} params;

void main() {
	/// Get pixel coordinate from global invocation ID
	/// gl_GlobalInvocationID.xy gives us unique pixel coordinates
	ivec2 coord = ivec2(gl_GlobalInvocationID.xy);

	/// Get image dimensions
	ivec2 size = imageSize(outputImage);

	/// Early exit if thread is outside image bounds
	/// This handles cases where work group size doesn't perfectly divide image size
	if (coord.x >= size.x || coord.y >= size.y) {
		return;
	}

	/// Convert pixel coordinate to UV (0.0 to 1.0)
	vec2 uv = vec2(coord) / vec2(size);

	/// Map UV to complex plane coordinates
	/// - Center at (0, 0) by subtracting 0.5
	/// - Scale by zoom factor
	/// - Offset to allow panning
	vec2 c = (uv - 0.5) * params.zoom + params.offset;

	/// Mandelbrot iteration
	/// Formula: z(n+1) = z(n)^2 + c, starting with z(0) = 0
	/// We iterate until |z| > 2 (escaped) or max iterations reached
	vec2 z = vec2(0.0);
	int iter = 0;

	for (int i = 0; i < params.maxIter; i++) {
		/// Complex square: (a + bi)^2 = (a^2 - b^2) + (2ab)i
		float zx2 = z.x * z.x;
		float zy2 = z.y * z.y;

		/// Check if escaped (|z|^2 > 4, which means |z| > 2)
		if (zx2 + zy2 > 4.0) {
			break;
		}

		/// z = z^2 + c
		z = vec2(zx2 - zy2, 2.0 * z.x * z.y) + c;
		iter++;
	}

	/// Color based on iteration count
	/// Normalize iteration count to 0.0-1.0
	float t = float(iter) / float(params.maxIter);

	/// Create smooth color gradient using cosine palette
	/// This creates beautiful rainbow-like colors
	vec3 color = 0.5 + 0.5 * cos(6.28318 * (t + vec3(0.0, 0.33, 0.67)));

	/// Points in the set (didn't escape) are black
	if (iter == params.maxIter) {
		color = vec3(0.0);
	}

	/// Write color to storage image
	/// This is the KEY function we're learning - imageStore()
	/// Format: imageStore(image, coordinate, value)
	imageStore(outputImage, coord, vec4(color, 1.0));
}
