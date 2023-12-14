#version 410

in vec3 fragPosWorld;

uniform vec3 lightPosWorld;

// uniform vec4 color;
uniform float nearPlane, farPlane;

void main()
{
	// Stores depth values for the shadowcast cubemap
	// to eventually be used by any objects using the ShadowMappedSurface shader system.

	float lightToFragDistance = length(fragPosWorld - lightPosWorld); // How far away is the fragment from the light?
	float relativeDepth = clamp((lightToFragDistance - nearPlane) / (farPlane - nearPlane), 0, 1); // What percentage of the Far->Near Plane distance is that distance?

	gl_FragDepth = relativeDepth;
}