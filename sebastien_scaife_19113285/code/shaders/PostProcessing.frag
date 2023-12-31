#version 410

in vec3 fragPosWorld;
in vec2 texCoord;

out vec4 colorOut;

uniform int renderMode;		// Determines which type of post-process to apply
uniform sampler2D albedo;	// This is the output texture from the post-process framebuffer i.e. ppColor

// Watercolor Uniforms
uniform sampler2D gauss;	// FOR WATERCOLOR: Gaussian Noise Map for Pigment Dispersion
uniform sampler2D simplex;	// FOR WATERCOLOR: Simplex Noise Map for Turbulent Flow
uniform sampler2D paper;	// FOR WATERCOLOR: Paper Layer for final pass

// Animated Color Shift Uniforms
uniform float animTime;		// FOR ANIMATED POSTPROCESSING: time of since the application started

// ----

// Take in a Source Colour and a Pigment Density Value d to return a more/less saturated Modified Color,
// such that d=1 => modified color = source color.
// Bousseau et al., 2006. 
vec4 BousseauColorMod(vec4 _srcColor, float _d)
{
	vec4 modifiedColor;

	modifiedColor = _srcColor - (_srcColor - (_srcColor * _srcColor)) * (_d-1);

	return modifiedColor;
}

void main()
{
	vec4 originalColour = texture(albedo, texCoord);

	switch(renderMode){
		case 1:
			// renderMode = 0 will be standard/ non-post-processed rendering.
			colorOut = originalColour;
			break;

		case 2:
			// renderMode = 1 will be the watercolour shader effect!

			float effectIntensity = 1;	// this is the value of k in [d = 1 - (k(T-0.5))] when detemining the density value d for Bousseau Color Modification
										// Basically, higher k means that colors will be saturation-shifted more than lower k
			
			float dimmingFactor = 0.8f;	// end result is slightly too bright if using the paper layer, this tones it down a little bit

			// read values in range [0,1] directly from noise maps
			float gaussVal = texture(gauss, texCoord).r;
			float simplexVal = texture(simplex, texCoord).r;
			vec4 paperColor = texture(paper, texCoord);

			// turn  the noise map values into pigment density values
			float dGauss = 1 - effectIntensity * (gaussVal - 0.5);
			float dSimplex = 1 - effectIntensity * (simplexVal - 0.5);

			// TODO do a bit of color warping here.
			vec4 pigmentDispersionPass = BousseauColorMod(originalColour, dGauss);
			vec4 turbulentFlowPass = BousseauColorMod(pigmentDispersionPass, dSimplex);

			// colorOut = turbulentFlowPass; // comment/uncomment this depending on where you arent/are using the paper layer.
			colorOut = (1 - (paperColor - turbulentFlowPass)) * dimmingFactor; // comment/uncomment this depending on where you are/arent using the paper layer.

			break;

		case 3: 
			// render mode 3 will smoothly transition from normal colours to inverted colours and back

			colorOut = (originalColour - 0.5) * cos(animTime) + 0.5;
			break;
		default:
			// failsafe in case renderMode somehow has a different value, it'll default back to standard rendering
			colorOut = originalColour;
			break;
	}
}