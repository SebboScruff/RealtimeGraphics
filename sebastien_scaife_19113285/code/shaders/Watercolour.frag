
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
