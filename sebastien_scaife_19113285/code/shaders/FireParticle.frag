
#version 410

in float flameTime;
in vec2 texCoords;
in float texXOffset;

uniform sampler1D flameColorTex;
uniform sampler2D flameAlphaTex;

out vec4 fragColor;

uniform float texWindowSize;
uniform float flameFadeinEnd;
uniform float flameFadeoutStart;

vec2 texCoordCenter = vec2(0.5f, 0.5f);

void main()
{
	// set flame color according to 1D color texture
	fragColor = vec4(1,1,1,1);
	fragColor.rgb = texture(flameColorTex,  flameTime).rgb;

	// Set flame alpha according to section of 2D alpha texture
	vec2 alphaGrab = vec2(texXOffset, flameTime);
	alphaGrab += texCoords * texWindowSize;
	// Need to use alphaGrab to get a texWindowSize section of flameAlphaMap
	fragColor.a = texture(flameAlphaTex, alphaGrab).r;

	// radial falloff function
	float distFromCenter = length(texCoords - texCoordCenter);
	fragColor.a /= distFromCenter;
}

