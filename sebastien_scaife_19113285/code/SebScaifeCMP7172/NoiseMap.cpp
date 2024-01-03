#include "NoiseMap.h"
#include "glhelper/Constants.hpp"
#include "SebScaifeCMP7172/OpenSimplexNoise.h"

NoiseMap::NoiseMap() {}
NoiseMap::~NoiseMap() {}

// Implementation of an OpenCV Gaussian Noise Generator, broadly inspired from https://pythonexamples.org/python-opencv-add-noise-to-image/ 
// and translated from a Python implementation to a C++ implementation.
// This is to be used as part of the Watercolor shader pipeline, to simulate Pigment Dispersion in the first Color Abstraction pass
cv::Mat NoiseMap::GenerateGaussianMap(int _width, int _height, int _cellSize, int _mean = 0, int _stanDev = 180, bool _outputFile)
{
	cv::Mat gaussianMap = cv::Mat::zeros(cv::Size(_width/_cellSize, _height/_cellSize), CV_64FC1); // creates an empty image with the incoming height and width
	cv::Mat outputMap;

	cv::randn(gaussianMap, _mean, _stanDev);

	cv::resize(gaussianMap, outputMap, cv::Size(_width, _height), 0, 0, cv::INTER_CUBIC);

	if (_outputFile) {
		cv::imwrite(outputFilePath + "gaussianMap.jpg", outputMap);
	}

	return outputMap;
}

// Implementation of a Perlin Noise Generator
// This is to be used as part of the Watercolor shader pipeline, to simulate Turbulent Flow in the second Color Abstraction pass
cv::Mat NoiseMap::GenerateSimplexMap(int _width, int _height, float _scale, bool _outputFile, int _seed)
{
	cv::Mat simplexMap = cv::Mat::zeros(cv::Size(_width, _height), CV_64FC1);
	OpenSimplexNoise::Noise noise(_seed);

	// TODO Look into Simplex Noise instead of Perlin to remove that visible grid effect
	for (int x = 0; x < _width; ++x) {
		for (int y = 0; y < _height; ++y) {
			double val = noise.eval(double(x)* _scale, double(y) * _scale); // range [-1,1]
			val *= 0.5; // range [-0.5, 0.5]
			val += 0.5; // range [0,1]
			val *= 255; // range [0,255]

			simplexMap.at<double>(y, x) = val;
		}
	}

	if (_outputFile) {
		cv::imwrite(outputFilePath + "simplexMap.jpg", simplexMap);
	}

	return simplexMap;
}

/////////////////////////////
// -- PRIVATE FUNCTIONS -- //
/////////////////////////////

// NOTE: Basically all of this is redundant now - gonna keep it in here just for reference,
// but the Turbulent Flow noise map is now Simplex Noise, managed by OpenSimplexNoise

// Basic linear interpolation: if 0 < t < 1, returns a value between a and b (provided a < b)
float NoiseMap::lerp(float a, float b, float t)
{
	if (t < 0) {
		return a;
	}
	else if (t > 1) {
		return b;
	}
	else {
		return (b - a) * t + a;
	}
}

// Create a pseudorandom direction vector
Eigen::Vector2f NoiseMap::randomGradient(int _ix, int _iy)
{
	const unsigned w = 8 * sizeof(unsigned);
	const unsigned s = w / 2;
	unsigned a = _ix, b = _iy;

	// do a bunch of arbitrary multiplication and bitwise operations
	// to generate a random float from the function parameters, in the range [0, 2PI]
	// Given the same inputs, it will always generate the same output vector
	a *= 235134323; b ^= a << s | a >> w - s;
	b *= 273491234; a ^= b << s | b >> w - s;
	a *= 834567342;

	float rand = a * (M_PI / ~(~0u >> 1)); // a is now a random number in the range [0, 2PI]

	Eigen::Vector2f v = Eigen::Vector2f(cos(rand), sin(rand));
	return v;
}

// Returns the Dot Product of a random gradient and a specific distance
// The distance here is the distance vector from (_ix, _iy) to (_x, _y)
float NoiseMap::dotGridGradient(int _ix, int _iy, float _x, float _y)
{
	Eigen::Vector2f gradient = randomGradient(_ix, _iy);
	Eigen::Vector2f distance = Eigen::Vector2f(_x - (float)_ix, _y - (float)_iy);

	return distance.dot(gradient);
}

// Return a perlin noise value at co-ordinates (_x, _y)
float NoiseMap::perlin(float _x, float _y)
{
	int x0 = (int)floor(_x);
	int x1 = x0 + 1;

	int y0 = (int)floor(_y);
	int y1 = y0 + 1;

	float weightX = _x - (float)x0;
	float weightY = _y - (float)y0;

	float n0, n1, ix0, ix1, val;
	n0 = dotGridGradient(x0, y0, _x, _y);
	n1 = dotGridGradient(x1, y0, _x, _y);
	ix0 = lerp(n0, n1, weightX);

	n0 = dotGridGradient(x0, y1, _x, _y);
	n1 = dotGridGradient(x1, y1, _x, _y);
	ix1 = lerp(n0, n1, weightX);

	val = lerp(ix0, ix1, weightY); // val is in range [-1, 1]
	val *= 0.5f; // val is in range [-0.5, 0.5]
	val += 0.5f; // val is in range [0, 1]
	val *= 255; // val is in range [0, 255], ready for openCV greyscale map

	return val;
}
