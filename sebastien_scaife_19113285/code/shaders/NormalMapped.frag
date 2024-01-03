/* A Lit Lambert that takes in a normal map to apply depth. */


#version 410

layout(std140) uniform cameraBlock
{
	mat4 worldToClip;
	vec4 cameraPos;
	vec4 cameraDir;
};
in vec2 texCoord;
in vec3 fragPosWorld;
in mat3 TBN;
in vec3 worldNorm;
in vec3 tangent;
in vec3 bitangent;

out vec4 colorOut;

uniform sampler2D albedo;	// diffuse map from texture file 
uniform sampler2D normalMap; // normal map from texture file

uniform vec3 lightPos;
uniform float lightIntensity;

vec3 lightDir;
vec3 viewDir;
float lightDist;
float specularity = 10.f; // this would probably be better as a shader uniform if more objects in the scene used normal mapping
float wrapLightFactor = 0.8f;
float baseLightFactor = 4.f;

void main()
{
	// value setup
	lightDir = normalize(lightPos - fragPosWorld);
	viewDir = normalize(cameraPos.xyz - fragPosWorld);
	lightDist = length(fragPosWorld - lightPos);

	float wrapLightIntensity = clamp((dot(lightDir, normalize(worldNorm) + wrapLightFactor)) / (1 + wrapLightFactor), 0, 1);

	vec4 albedoSample = texture(albedo, texCoord); // base colour sample from albedo map

	vec4 normalSample = texture(normalMap, texCoord); // corresponding normal data from normal map (current a vec4 in [0,1] range
	vec3 normalSampleScaled = vec3((normalSample.xyz * 2)-1); // scale the normal map data to the range [-1,1] in 3 dimensions for N, T, and B
	vec3 normalFromMap = (normalSampleScaled.x * tangent, normalSampleScaled.y * bitangent, normalSampleScaled.z * worldNorm);

	// Set base color to albedo sample
	vec4 baseColor = albedoSample * dot(worldNorm, lightDir);
	baseColor.a = 1.0f;

	float specPower = pow(clamp(dot(reflect(lightDir, normalFromMap), -viewDir), 0, 1), specularity);
	vec4 specColour = vec4(specPower * vec3(1,1,1), 1.0);

	colorOut = baseColor + specColour;

	// Lighting Pass
	colorOut *= lightIntensity * baseLightFactor;
	colorOut *= wrapLightIntensity;
	colorOut /= (lightDist) * (lightDist);

	colorOut.a =  1.0;
}
