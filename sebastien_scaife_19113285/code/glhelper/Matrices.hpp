#pragma once

#include <Eigen/Dense>
#include <cmath>

Eigen::Matrix4f perspective(float fov, float aspect, float zNear, float zFar);

Eigen::Matrix4f makeTranslationMatrix(const Eigen::Vector3f& translate);

Eigen::Matrix4f makeScaleMatrix(float scale);

// Seb Addition, for a single method to combine rotation in multiple axes
Eigen::Matrix4f makeRotationMatrix(const Eigen::Vector3f& eulerAngles);
// End of Seb Addition

