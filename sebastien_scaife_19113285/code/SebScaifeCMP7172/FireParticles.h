#pragma once
#include "glhelper/Mesh.hpp"
#include "glhelper/ShaderProgram.hpp"

// Class to contain all the particle placement maths for the fire particle system.

class FireParticles : public glhelper::Mesh {
public:
	FireParticles(std::string name, const Eigen::Matrix4f& modelToWorld = Eigen::Matrix4f::Identity());
	~FireParticles() throw();

	void InitialiseLocations();
	Eigen::Vector4f SolveForFlameCubic();

	float GetDuration();
	float GetHeight();
	float GetTexWindowSize();
	float GetFadeoutStart();
	float GetFadeinEnd();

private:
	glhelper::ShaderProgram* shaderProgram_;
	GLuint vao_;
	size_t nElems_, nVerts_;
	GLenum drawMode_;

	// --- Flame control parameters ---
	int nFireParticles = 300; // No. of particles in the fire total.
	float fireBaseRadius = 0.6f; // Radius of the base of the fire. Determines initial positions of particles.

	float flameDuration = 2.f; // How long a particle takes to reach the top of the flame
	float flameHeight = 1.5f; // Height of the flame
	float flameFadeinEnd = 0.1f; // Up to this position in the flame, the particles linearly fade in (proportion, in [0,1]).
	float flameFadeoutStart = 0.5f; // At this position in the flame the particles will linearly fade out (proportion, in [0,1]).

	float flameTexWindowSize = 0.2f; // Determines the size of the windowed region of the alpha texture each
	// flame particle samples from. Should be between 0 and 1.

	// Parameters controlling curve of flame.
	float flameBulgeHeight = 0.35f; // The height at which the flame is the widest (proportion, in [0,1])
	float flameBulgeRadius = 0.65f; // The radius at the widest point (multiple of the initial radius at the base of the flame)
	float flameTopRadius = 0.25f; // The radius the flame narrows to at the top (again multiple of initial radius)
};
