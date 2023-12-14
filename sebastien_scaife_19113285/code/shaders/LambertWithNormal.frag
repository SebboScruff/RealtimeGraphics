/* A Lit Lambert that takes in a normal map to apply depth. */


#version 410

layout(std140) uniform cameraBlock
{
	mat4 worldToClip;
	vec4 cameraPos;
	vec4 cameraDir;
};
in vec3 worldNorm;
in vec3 fragPosWorld;
in vec2 texCoord;

out vec4 colorOut;

uniform sampler2D albedo;	// diffuse map from texture file 
uniform sampler2D normalMap; // normal map from texture file
uniform vec3 lightPos;
uniform float lightIntensity;

vec3 lightDir;
float lightDist;
float wrapLightFactor = 0.8f;
float baseLightFactor = 2.f;

void main()
{
	// value setup
	lightDir = normalize(lightPos - fragPosWorld);
	lightDist = length(fragPosWorld - lightPos);

	float wrapLightIntensity = clamp((dot(lightDir, normalize(worldNorm) + wrapLightFactor)) / (1 + wrapLightFactor), 0, 1);

	vec4 albedoSample = texture(albedo, texCoord);

	// Set base color to albedo sample
	colorOut = albedoSample;

	// Lighting Pass
	colorOut *= lightIntensity * baseLightFactor;
	colorOut *= wrapLightIntensity;
	colorOut /= (lightDist) * (lightDist);

	colorOut.a =  1.0;
}
