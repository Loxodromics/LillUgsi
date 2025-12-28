#version 450

/// Storage buffer test compute shader
/// Writes a predictable pattern to verify compute pipeline works correctly

/// Work group size: 64 threads in X dimension
/// This is a common size that works well on most GPUs
/// 64 is typically the "wavefront" or "warp" size on AMD/NVIDIA
layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

/// Storage buffer declaration
/// Key difference from uniform buffers:
/// - Uses 'buffer' keyword instead of 'uniform'
/// - Allows read AND write access (uniforms are read-only)
/// - Uses VK_DESCRIPTOR_TYPE_STORAGE_BUFFER on the Vulkan side
layout(set = 0, binding = 0) buffer OutputBuffer {
	float values[];
};

void main() {
	/// gl_GlobalInvocationID.x gives each thread a unique index
	/// across ALL work groups (not just within one group)
	///
	/// With local_size_x = 64 and 4 work groups dispatched:
	/// - Work group 0: threads 0-63
	/// - Work group 1: threads 64-127
	/// - Work group 2: threads 128-191
	/// - Work group 3: threads 192-255
	uint idx = gl_GlobalInvocationID.x;

	/// Write a predictable pattern: value = index * 2
	/// This lets us verify on CPU that each thread wrote correctly
	values[idx] = float(idx) * 2.0;
}
