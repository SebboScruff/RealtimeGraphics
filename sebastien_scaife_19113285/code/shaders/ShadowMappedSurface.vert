#version 410

layout(location = 0) in vec3 vPos;
layout(location = 1) in vec3 vNorm;
layout(location = 2) in vec2 vTex;

layout(std140) uniform cameraBlock
{
	mat4 worldToClip;
	vec4 cameraPos;
	vec4 cameraDir;
};
uniform mat4 modelToWorld;
uniform mat3 normToWorld;

out vec3 fragPosWorld;
out vec3 worldNorm;
out vec2 texCoord;

void main()
{
	texCoord = vTex;

	gl_Position = worldToClip *  modelToWorld * vec4(vPos, 1.0f);
	fragPosWorld = (modelToWorld * vec4(vPos, 1.0f)).xyz;
	worldNorm = normToWorld * vNorm;
}