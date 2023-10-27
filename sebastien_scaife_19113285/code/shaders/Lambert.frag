
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

uniform vec4 albedo;

uniform vec3 lightPosWorld;
uniform float lightIntensity;

void main()
{
	float lightDistance = length(lightPosWorld - fragPosWorld);

	colorOut = albedo;

	// Adjust for lighting variables
	colorOut *= lightIntensity;
	colorOut /= (lightDistance * lightDistance);
}

