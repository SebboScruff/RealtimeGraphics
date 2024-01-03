#include "FireParticles.h"
#include <random>

FireParticles::FireParticles(std::string name, const Eigen::Matrix4f& modelToWorld) : Mesh(name, modelToWorld),
shaderProgram_(nullptr),
nElems_(0), nVerts_(0),
drawMode_(GL_POINTS) // using draw mode: points because the input to the geometry shader is a point cloud
{
	glGenVertexArrays(1, &vao_);
	InitialiseLocations();
}

FireParticles::~FireParticles() throw()
{
	glDeleteVertexArrays(1, &vao_);
}

void FireParticles::InitialiseLocations()
{
	std::vector<Eigen::Vector3f> fireInitParticlePositions(nFireParticles);
	std::vector<Eigen::Vector2f> fireRandSeedValues(nFireParticles);
	std::random_device dev;
	std::default_random_engine eng(dev());
	std::uniform_real_distribution<> dist(-fireBaseRadius, fireBaseRadius);
	std::uniform_real_distribution<> dist01(0.f, 1.f);
	// This uses rejection sampling
	// Generate random points in a square, then reject those that
	// don't fall within the desired circle.
	for (size_t i = 0; i < nFireParticles; ++i) {
		do
			fireInitParticlePositions[i] = Eigen::Vector3f(dist(eng), 0.f, dist(eng));
		while (fireInitParticlePositions[i].norm() > fireBaseRadius);

		fireRandSeedValues[i] = Eigen::Vector2f(dist01(eng), dist01(eng));
	}
	vert(fireInitParticlePositions);
	tex(fireRandSeedValues);
}

// Use of Polynomial Regression to find the coefficients of a cubic equation
// that satisfies the control parameters in the header file.
Eigen::Vector4f FireParticles::SolveForFlameCubic()
{
	Eigen::Matrix<float, 3, 4> X;
	X <<
		1.0, 0.0, 0.0, 0.0,
		1.0, 1.0, 1.0, 1.0,
		1.0, flameBulgeHeight, flameBulgeHeight* flameBulgeHeight, flameBulgeHeight* flameBulgeHeight* flameBulgeHeight;
	Eigen::Vector3f y(1.0, flameTopRadius, flameBulgeRadius);

	// Using pseudo-inverse here, as this isn't a square matrix.
	Eigen::Vector4f solution = (X.transpose() * X).completeOrthogonalDecomposition().pseudoInverse() * X.transpose() * y;
	return solution;
}

float FireParticles::GetDuration()
{
	return flameDuration;
}

float FireParticles::GetHeight()
{
	return flameHeight;
}

float FireParticles::GetTexWindowSize()
{
	return flameTexWindowSize;
}

float FireParticles::GetFadeoutStart()
{
	return flameFadeoutStart;
}

float FireParticles::GetFadeinEnd()
{
	return flameFadeinEnd;
}


