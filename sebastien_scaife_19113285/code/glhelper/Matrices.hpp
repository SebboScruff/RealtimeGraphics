#pragma once

#include <Eigen/Dense>
#include <cmath>

Eigen::Matrix4f perspective(float fov, float aspect, float zNear, float zFar);

Eigen::Matrix4f makeTranslationMatrix(const Eigen::Vector3f& translate);

Eigen::Matrix4f makeScaleMatrix(float scale);
// Seb Addition, to allow for uneven scaling
Eigen::Matrix4f makeScaleMatrix(float scaleX, float scaleY, float scaleZ);
// End of Seb Addition

// Seb Addition, for a single method to combine rotation in multiple axes

Eigen::Matrix4f makeRotationMatrix(const Eigen::Vector3f& eulerAngles);

// End of Seb Addition

// Seb Addition, to make a single Transformation Matrix in one go

Eigen::Matrix4f makeFullTransformationMatrix(float _translateX, float _translateY, float _translateZ, float _rotateX, float _rotateY, float _rotateZ, float _scaleX, float _scaleY, float _scaleZ);

