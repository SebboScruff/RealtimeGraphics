
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

uniform vec4 albedo;			// diffuse map from texture file 
uniform vec4 specularIntensity; // metallic map from texture file
uniform float specularExponent;

uniform vec3 lightPosWorld;
uniform float lightIntensity;

void main()
{
	// Set up necessary values
	vec3 lightDir = normalize(lightPosWorld - fragPosWorld);
	vec3 reflectDir = lightDir - 2*(dot(lightDir, normalize(worldNorm))) * normalize(worldNorm);

	vec3 viewDir = normalize(fragPosWorld - cameraPos.xyz);
	viewDir *= -1;

	vec3 halfwayVec = normalize(lightDir) + normalize(viewDir);
	halfwayVec = normalize(halfwayVec);

	float lightDistance = length(lightPosWorld - fragPosWorld);

	// Render with a Normalised, Modified Blinn-Phong shader
	float normFactor = (specularExponent + 8)/8;
	colorOut = clamp(dot(lightDir, normalize(worldNorm)), 0, 1) *
				(albedo + specularIntensity * normFactor * pow(clamp(dot(halfwayVec, normalize(worldNorm)), 0, 1), specularExponent * 2));

	// Adjust for lighting variables
	colorOut *= lightIntensity;
	colorOut /= (lightDistance * lightDistance);
}
