#version 410

out vec4 colorOut;

in vec3 fragPosWorld;
in vec3 worldNorm;
in vec2 texCoord;

uniform sampler2D color;
uniform samplerCube shadowMap;
uniform vec3 lightPosWorld;
uniform float nearPlane, farPlane;
uniform float bias;

float baseLightIntensityFactor = 1.f;

void main()
{
	// Set up helper values
	vec3 lightDir = normalize(lightPosWorld - fragPosWorld);
	float lightDot = clamp(dot(lightDir, normalize(worldNorm)), 0, 1); // adjust color values on the objects with this shader 
	vec3 colorRgb = texture(color, texCoord).rgb * lightDot * baseLightIntensityFactor;

	// Shadow Test - check distance from a light source to a) the fragment in question and b) the nearest fragment in the scene.
	// if the fragment being shaded is not the closest one in the scene to the light, scale down the color intensity (aka cast a shadow)
	float lightToFragDistance = distance(lightPosWorld, fragPosWorld);
	float lightToNearest = texture(shadowMap, -lightDir).r * (farPlane - nearPlane) + nearPlane; // read from depth map to find the nearest point from the light to any mesh

	if(lightToFragDistance > lightToNearest + bias){
		colorRgb *= 0.2f;
	}

	colorOut.rgb = colorRgb;
	colorOut.a = 1.0;
}