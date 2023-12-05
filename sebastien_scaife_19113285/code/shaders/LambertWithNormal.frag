
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

void main()
{
	vec4 albedoSample = texture(albedo, texCoord);

	// Render out with value from albedo texture
	colorOut = albedoSample;
}

