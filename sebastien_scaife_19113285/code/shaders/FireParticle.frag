
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
	// Implement your fragment shader here.
	// To get the colour for the flame, use the flameColorTex, sampling according to the height
	// For the alpha, sample from flameAlphaTex.
	// Sample in a small square window of the flameAlphaTex texture, moving this window up along the
	// texture as the particle rises.
	// For both of these, use flameTime (note this is from 0 to 1, where 0 is bottom of fire, 1 is top).
	// Finally add a radial falloff to your alpha to get rid of those boxy edges (think about how to do this,
	// construct a function that falls off based on the distance from the centre of the quad, which is at 
	// texture coordinate (0.5, 0.5)).

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

