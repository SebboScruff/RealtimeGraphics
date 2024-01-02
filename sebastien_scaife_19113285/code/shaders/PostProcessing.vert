
#version 410

layout(location = 0) in vec3 vPos;
layout(location = 2) in vec2 vTex;

uniform mat4 modelToWorld;
uniform mat3 normToWorld;

out vec2 texCoord;
out vec3 fragPosWorld;

void main()
{
	//gl_Position = worldToClip *  modelToWorld * vec4(vPos, 1.0f);
	gl_Position = vec4(vPos, 1.0f);

	texCoord = vTex;
	//fragPosWorld = (modelToWorld * vec4(vPos, 1.0)).xyz;
	fragPosWorld = (vec4(vPos, 1.0)).xyz;
}