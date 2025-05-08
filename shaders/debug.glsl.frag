#version 450

/// Input from vertex shader
layout(location = 0) in vec3 fragColor;      /// For vertex color mode
layout(location = 1) in vec3 fragNormal;     /// For normal visualization
layout(location = 2) in vec3 fragPosition;   /// For winding order visualization
layout(location = 3) in vec3 fragViewDir;    /// For view-dependent effects

/// Output color
layout(location = 0) out vec4 outColor;

/// Debug material properties (set = 2)
/// We use a simple uniform buffer to allow runtime visualization adjustments
layout(set = 2, binding = 0) uniform DebugUBO {
	vec3 colorMultiplier;
	int visualizationMode;    /// 0=VertexColors, 1=NormalColors, 2=WindingOrder
} debug;

/// Camera uniform buffer (set = 0)
/// We need camera position for winding order visualization
layout(set = 0, binding = 0) uniform CameraUBO {
	mat4 view;
	mat4 proj;
	vec3 cameraPos;
} camera;

void main() {
	/// Choose visualization mode based on uniform value
	/// Each mode shows a different aspect of the mesh for debugging
	if (debug.visualizationMode == 0) {
		/// Vertex Colors Mode
		/// Shows the raw vertex colors assigned in the mesh
		vec3 finalColor = fragColor * debug.colorMultiplier;
		outColor = vec4(finalColor, 1.0);
	}
	else if (debug.visualizationMode == 1) {
		/// Normal Colors Mode
		/// Visualizes normals by mapping their XYZ components to RGB colors
		/// This gives an intuitive view of normal orientation
		/// We remap from [-1,1] to [0,1] range for display
		vec3 normalColor = normalize(fragNormal) * 0.5 + 0.5;
		outColor = vec4(normalColor, 1.0);
	}
	else if (debug.visualizationMode == 2) {
		/// Winding Order Visualization
		/// Shows which direction the faces are facing relative to the viewer
		/// Green = front face (facing camera), Red = back face (facing away)

		/// Calculate face normal using derivatives
		/// This gives us the true geometric normal of the rendered triangle
		vec3 dpdx = dFdx(fragPosition);
		vec3 dpdy = dFdy(fragPosition);
		vec3 faceNormal = normalize(cross(dpdx, dpdy));

		/// Calculate how much the face is facing the camera
		/// Positive values mean the face normal points toward the camera
		float facingRatio = dot(faceNormal, fragViewDir);

		/// Visualize front/back faces with color
		/// Green for front-facing triangles, red for back-facing triangles
		vec3 windingColor = facingRatio > 0.0 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);

		/// Apply color multiplier for adjusting brightness
		outColor = vec4(windingColor * debug.colorMultiplier, 1.0);
	}
	else {
		/// Fallback for unknown modes
		/// Use a distinctive purple color to indicate invalid mode
		outColor = vec4(1.0, 0.0, 1.0, 1.0);
	}
}