
#version 410

layout(location = 0) in vec3 vPos;
layout(location = 1) in vec3 vNorm;
layout(location = 2) in vec2 vTex;
layout(location = 4) in vec3 vTan;
layout(location = 5) in vec3 vBitan;

layout(std140) uniform cameraBlock
{
	mat4 worldToClip;
	vec4 cameraPos;
	vec4 cameraDir;
};
uniform mat4 modelToWorld;
uniform mat3 normToWorld;

out vec2 texCoord;
out vec3 fragPosWorld;
out mat3 TBN;
out vec3 worldNorm;
out vec3 tangent;
out vec3 bitangent;

void main()
{
	gl_Position = worldToClip *  modelToWorld * vec4(vPos, 1.0f);

	texCoord = vTex;
	fragPosWorld = (modelToWorld * vec4(vPos, 1.0)).xyz;

	worldNorm = normalize(normToWorld * vNorm);
	tangent = normalize(normToWorld * vTan);
	bitangent = normalize(normToWorld * vBitan);
	TBN = mat3(worldNorm, tangent, bitangent);
}