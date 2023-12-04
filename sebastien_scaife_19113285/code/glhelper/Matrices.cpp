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

// Seb Addition - Function Override for making uneven scale matrices
// This one can just be called if a model needs to be scaled differently in different axes
Eigen::Matrix4f makeScaleMatrix(float scaleX, float scaleY, float scaleZ)
{
	Eigen::Matrix4f scaleMat = Eigen::Matrix4f::Identity();

	scaleMat(0, 0) = scaleX;
	scaleMat(1, 1) = scaleY;
	scaleMat(2, 2) = scaleZ;
	scaleMat(3, 3) = 1.0f;

	return scaleMat;
}
// End of Seb Addition

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

// Seb Addition - make a full rotation matrix at once
Eigen::Matrix4f makeFullTransformationMatrix(float _translateX, float _translateY, float _translateZ, float _rotateX, float _rotateY, float _rotateZ, float _scaleX, float _scaleY, float _scaleZ)
{
	Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();

	transform *= makeTranslationMatrix(Eigen::Vector3f(_translateX, _translateY, _translateZ));
	transform *= makeRotationMatrix(Eigen::Vector3f(_rotateX, _rotateY, _rotateZ));
	transform *= makeScaleMatrix(_scaleX, _scaleY, _scaleZ);

	return transform;
}

// End of Seb Addition
