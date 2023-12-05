
#version 410

layout(points) in;
layout(triangle_strip, max_vertices=4) out;

in VertexData {
	float texXOffset;
	float flameTime;
} VertexIn[];


layout(std140) uniform cameraBlock
{
	mat4 worldToClip;
	vec4 cameraPos;
	vec4 cameraDir;
};

const float flameParticleSize = 0.5;

out float flameTime;
out vec2 texCoords;
out float texXOffset;

void main()
{
	// Write your geometry shader here!
	// This should generate a quad (4 vertices, forming a triangle_strip).
	// Set the data for each vertex, then call EmitVertex() to add it to the strip.
	// Call EndPrimitive() once you've added all 4 vertices.
	// Remember to use cross products to find your across and up vectors for the quad.
	// The size of the quad should be based on flameParticleSize.
	// Don't forget to pass through the other important info, like flameTime, texCoords,
	// texXOffset.
	// ---------------------

	vec4 camToPoint4 = gl_in[0].gl_Position - cameraPos;
	vec3 camToPoint3 = camToPoint4.xyz;

	vec3 worldUp = vec3(0.f, 1.f, 0.f); // arbitrary up vector to get first perpendicular vector to (Cam->Point)
	vec3 across = normalize(cross(worldUp, camToPoint3)); // cross (Cam->Point) with arbitrary vector to get perpendicular across vector
	vec3 up = normalize(cross(across, camToPoint3)); // cross (Cam->Point) with perpendicular across vector to get perpendicular up vector

	// Nested For loop to cycle through the corners of a square
	// used to set up the billboard texture for the particles
	for(int x = 0; x < 2; ++x){
		for(int y = 0; y < 2; ++y){
			// Define each vertex of the billboard as (particle location) ± 0.5 in x and y, relative to the camera
			// This sets up a billboard quad that faces the camera
			vec3 vertPos = gl_in[0].gl_Position.xyz;
			vertPos += ((float(x) - 0.5f)*across + (float(y) - 0.5f)*up) * flameParticleSize;
			// Translate billboard vertices to Clip Space
			gl_Position = worldToClip*vec4(vertPos, 1);

			// Set the rest of the vertex parameters
			flameTime = VertexIn[0].flameTime;
			texXOffset = VertexIn[0].texXOffset;
			texCoords = vec2(x,y);
			// Finish and emit this vertex
			EmitVertex();
		}
	}
	// Once the above code has been run for BL/BR/TL/TR corners, the quad is finished
	// and the geometry shader can move on to the next particle point.
	EndPrimitive();
}

