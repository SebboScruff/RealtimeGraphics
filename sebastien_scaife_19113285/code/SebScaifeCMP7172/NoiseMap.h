#pragma once

#include <opencv2/opencv.hpp>
#include <cmath>
#include <Eigen/Dense>

class NoiseMap
{
public:
	NoiseMap();
	~NoiseMap();

	cv::Mat GenerateGaussianMap(int _width, int _height, int _cellSize, int _mean, int _stanDev, bool _outputFile = false);

	cv::Mat GenerateSimplexMap(int _width, int _height, float _scale, bool _outputFile = false, int _seed = 738274829357);

private:
	std::string outputFilePath = "../../bin/Assets/Textures/wcRefs/noiseMaps/"; // where are the files dropped if the outputFile bool is true

	float lerp(float a, float b, float t);

	Eigen::Vector2f randomGradient(int _ix, int _iy);

	float dotGridGradient(int _ix, int _iy, float _x, float _y);

	float perlin(float _x, float _y);
};