#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragPosition;
layout(location = 3) in vec2 fragTexCoord;
layout(location = 4) in mat3 fragTBN;
layout(location = 7) in vec3 fragViewDir;

layout(location = 0) out vec4 outColor;

/// Light structure (same as PBR)
struct Light {
	vec4 direction;
	vec4 colorAndIntensity;
	vec4 ambient;
};

layout(set = 1, binding = 0) uniform LightBuffer {
	Light lights[16];
	uint lightCount;
} lightData;

/// Snow material properties
layout(set = 2, binding = 0) uniform SnowMaterial {
	vec4 baseAlbedo;
	float darkeningAmount;
	float darkeningPower;
	float fresnelPower;
	float rimIntensity;
	vec3 skyColor;
	float _pad1;
	float sparkleScale;
	float sparkleThreshold;
	float sparkleIntensity;
	float _pad2;
} snow;

const float PI = 3.14159265359;
const float EPSILON = 0.0001;

/// Hash functions for sparkle
float hash3to1(vec3 p) {
	return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453);
}

vec3 hash3to3(vec3 p) {
	p = vec3(dot(p, vec3(127.1, 311.7, 74.7)),
			 dot(p, vec3(269.5, 183.3, 246.1)),
			 dot(p, vec3(113.5, 271.9, 124.6)));
	return fract(sin(p) * 43758.5453);
}

void main() {
	vec3 N = normalize(fragTBN[2]);  /// World normal from TBN
	vec3 V = normalize(fragViewDir);

	float NdotV = max(dot(N, V), 0.0);

	/// 1. View-dependent albedo darkening
	float viewDarkening = mix(1.0, snow.darkeningAmount,
							  pow(1.0 - NdotV, snow.darkeningPower));
	vec3 albedo = snow.baseAlbedo.rgb * viewDarkening;

	/// 2. Fresnel rim lighting
	float fresnel = pow(1.0 - clamp(NdotV, 0.0, 1.0), snow.fresnelPower);
	vec3 rimLight = fresnel * snow.skyColor * snow.rimIntensity;

	/// Initialize output
	vec3 finalColor = vec3(0.0);
	vec3 ambientColor = vec3(0.0);

	/// Process lights
	for (int i = 0; i < int(lightData.lightCount); i++) {
		Light light = lightData.lights[i];
		float lightIntensity = light.colorAndIntensity.w;
		if (lightIntensity < EPSILON) continue;

		vec3 lightColor = light.colorAndIntensity.rgb * lightIntensity;
		vec3 L = normalize(-light.direction.xyz);

		float NdotL = max(dot(N, L), 0.0);

		/// Accumulate ambient
		ambientColor += light.ambient.rgb * lightIntensity;

		/// Lambert diffuse (energy conserving)
		vec3 diffuse = albedo / PI * NdotL;

		/// 3. Sparkle effect
		vec3 noiseCoord = fragPosition * snow.sparkleScale;
		vec3 cellCoord = floor(noiseCoord);
		float sparkleNoise = hash3to1(cellCoord);
		vec3 microNormal = normalize(hash3to3(cellCoord) * 2.0 - 1.0);

		vec3 H = normalize(L + V);
		float alignment = max(dot(microNormal, H), 0.0);
		float sparkle = smoothstep(snow.sparkleThreshold, 1.0, alignment);
		sparkle *= clamp(NdotL * 4.0, 0.0, 1.0);  /// Mask by light facing

		vec3 sparkleColor = lightColor * snow.sparkleIntensity * sparkle * sparkleNoise;

		/// Combine for this light
		finalColor += diffuse * lightColor + sparkleColor;
	}

	/// Add ambient and rim
	finalColor += ambientColor * albedo;
	finalColor += rimLight;

	/// Tone mapping (Reinhard)
	finalColor = finalColor / (finalColor + vec3(1.0));

	outColor = vec4(finalColor, snow.baseAlbedo.a);
}
