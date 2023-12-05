
#version 410

uniform sampler2D albedoTexture;
uniform sampler1D lightingTexture;
// Need to bring in a sampler2D each for:
// Turbulent Flow: Simplex Noise Map
uniform sampler2D turbulentFlowMap;
// Pigment Distribution: Gaussian Noise Map
uniform sampler2D pigmentDistributionMap;
// (Paper Layer: Pre-Loaded Paper Texture)

// also need a uniform float for colourModStrength (variable k in the formulae below)

layout(std140) uniform cameraBlock
{
	mat4 worldToClip;
	vec4 cameraPos;
	vec4 cameraDir;
};
in vec3 worldNorm;
in vec3 fragPosWorld;
vec2 fragPosScreen;
in vec2 texCoord;

out vec4 colorOut;
uniform vec4 color;
uniform vec3 lightPosWorld;
	
void main()
{
	vec3 albedo = texture(albedoTexture, texCoord).xyz;

	// Stage 1: Lighting Abstraction
	// Modulate the lighting using the preloaded sampler1D lightingTexture.
	vec3 lightDir = normalize(lightPosWorld - fragPosWorld);
	float lightingFactor = clamp(dot(lightDir, normalize(worldNorm)), 0, 1);
	float lighting = texture(lightingTexture, lightingFactor).r;

	colorOut.xyz = albedo;
	colorOut *= lighting;
	colorOut.a = 1.0;
	// NOTE: Leaving colorOut as it is here is a basic Cel-Shader

	// Stage 2: Colour Abstraction
	// Alter the colorOut using Bousseau's Colour Modification Formula:
	// 1) Find fragment position in Screen Space
	// 2) Take Perlin & Gaussian texture Coordinates from fragPosScreen to get values for T(Turbulence) and T(Dispersion), respectively
	// 3) [d = 1 - k(T-0.5)] to get d(Turbulence) and d(Dispersion)
	// 4) Run Colour Mod Formula C’ = C – (C-C^2)(d-1) once with d(Turb), then once again with d(Disp)
	// This should give a pretty good estimation for colorOut!

	// (paper layer is then run as a post-process)

}

// Implementation of Bousseau, A., et al.'s (2006)
// Pigment-density color manipulation
// if density = 1, returns the Source Color
vec3 bousseauColorShift(vec3 _srcColor, float _density)
{
	vec3 outputColor = _srcColor - (_srcColor - pow(_srcColor, 2)) * (_density - 1);
	return outputColor;
}

