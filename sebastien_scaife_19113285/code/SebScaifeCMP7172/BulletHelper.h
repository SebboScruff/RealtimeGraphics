#pragma once

#include "glhelper/Matrices.hpp"
#include "bullet/btBulletCollisionCommon.h"
#include "bullet/btBulletDynamicsCommon.h"

class BulletHelper
{
public:
	void AddBoxShape(btAlignedObjectArray<btCollisionShape*>& shapes, btDiscreteDynamicsWorld* world, const Eigen::Vector3f& position, const Eigen::Vector3f& size, float mass, float restitution, btVector3 _localIntertia);

	void AddCylinderShape(btAlignedObjectArray<btCollisionShape*>& shapes, btDiscreteDynamicsWorld* world, const Eigen::Vector3f& position, const Eigen::Vector3f& size, float mass, float restitution, btVector3 _localIntertia);

	Eigen::Matrix4f getRigidBodyTransform(btDynamicsWorld* world, int idx);

private:
};