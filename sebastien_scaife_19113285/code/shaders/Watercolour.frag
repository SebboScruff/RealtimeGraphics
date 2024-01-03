
#version 410

uniform sampler2D albedoTexture;
uniform sampler1D lightingTexture;

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
uniform vec3 lightPosWorld;

// Take in a Source Colour and a Pigment Density Value d to return a more/less saturated Modified Color,
// such that d=1 => modified color = source color.
// Bousseau et al., 2006. 
vec4 BousseauColorMod(vec4 _srcColor, float _d)
{
	vec4 modifiedColor;

	modifiedColor = _srcColor - (_srcColor - (_srcColor * _srcColor)) * (_d-1);

	return modifiedColor;
}

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

	// Stage 2: Colour Abstraction is done in PostProcessing.frag!
}
