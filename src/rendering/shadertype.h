#pragma once

#include <string>
#include <vector>

namespace lillugsi::rendering {

/// ShaderPaths holds the file paths for different shader stages
/// We use this structure to:
/// 1. Separate shader path configuration from shader creation
/// 2. Allow for future expansion to other shader types
/// 3. Enable pipeline manager to handle shader resource creation
struct ShaderPaths {
	/// Path to the vertex shader SPIR-V file
	/// This shader defines the vertex processing stage
	std::string vertexPath;

	/// Path to the fragment shader SPIR-V file
	/// This shader defines the fragment processing stage
	std::string fragmentPath;

	/// Validate that the shader paths are set
	/// We need this to ensure materials provide valid shader configurations
	/// @return true if both vertex and fragment shader paths are non-empty
	[[nodiscard]] bool isValid() const {
		return !vertexPath.empty() && !fragmentPath.empty();
	}
};

} /// namespace lillugsi::rendering