#include "Matrices.hpp"

Eigen::Matrix4f perspective(float fov, float aspect, float zNear, float zFar)
{
	float tanHalfFovy = tan(fov * 0.5f);

	Eigen::Matrix4f result = Eigen::Matrix4f::Zero();
	result(0, 0) = 1.0f / (aspect * tanHalfFovy);
	result(1, 1) = 1.0f / tanHalfFovy;
	result(2, 2) = (zFar + zNear) / (zNear - zFar);
	result(3, 2) = -1.0f;
	result(2, 3) = (2.0f * zFar * zNear) / (zNear - zFar);

	return result;
}

Eigen::Matrix4f makeTranslationMatrix(const Eigen::Vector3f& translate)
{
	Eigen::Matrix4f matrix = Eigen::Matrix4f::Identity();
	matrix.block<3, 1>(0, 3) = translate;
	return matrix;
}

Eigen::Matrix4f makeScaleMatrix(float scale)
{
	Eigen::Matrix4f scaleMat = Eigen::Matrix4f::Identity() * scale;
	scaleMat(3, 3) = 1.0f;
	return scaleMat;
}

// Seb Addition

// Ensure the incoming Euler Angles are in Radians, not Degrees!
Eigen::Matrix4f makeRotationMatrix(const Eigen::Vector3f& _eulerAngles)
{
	float xRot = _eulerAngles.x(), yRot = _eulerAngles.y(), zRot = _eulerAngles.z();
	Eigen::Matrix4f rotMat = Eigen::Matrix4f::Identity();

	Eigen::Matrix3f r;
	r = Eigen::AngleAxisf(zRot, Eigen::Vector3f::UnitZ()).matrix()
		* Eigen::AngleAxisf(yRot, Eigen::Vector3f::UnitY()).matrix()
		* Eigen::AngleAxisf(xRot, Eigen::Vector3f::UnitX()).matrix();
	
	rotMat.block<3, 3>(0, 0) = r;

	//rotMat(3, 3) = 1.0f;
	return rotMat;
}
// End of Seb Addition
