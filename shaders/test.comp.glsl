#version 450

/// Test compute shader that generates a simple gradient pattern
/// This shader writes to a storage image, demonstrating basic compute functionality

/// Define the work group size
/// 16x16 = 256 threads per work group, a good default for most GPUs
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

/// Output storage image
/// rgba8 format for RGBA with 8 bits per channel
/// writeonly because we only write to this image, never read from it
layout(set = 0, binding = 0, rgba8) uniform writeonly image2D outputImage;

void main() {
	/// gl_GlobalInvocationID gives us the unique thread ID across all work groups
	/// This is our pixel coordinate
	ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);

	/// Get image dimensions to normalize coordinates
	ivec2 imgSize = imageSize(outputImage);

	/// Create a simple gradient based on position
	/// X controls red, Y controls green
	vec4 color = vec4(
		float(pixelCoord.x) / float(imgSize.x),  /// Red increases left to right
		float(pixelCoord.y) / float(imgSize.y),  /// Green increases top to bottom
		0.0,                                      /// No blue
		1.0                                       /// Full alpha
	);

	/// Write the color to the output image
	imageStore(outputImage, pixelCoord, color);
}
