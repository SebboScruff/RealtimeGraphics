#pragma once
#include "bullet/btBulletCollisionCommon.h"
#include "bullet/btBulletDynamicsCommon.h"
#include "Eigen/Dense"

class BulletHelper
{
public:
	void AddBoxShape(btAlignedObjectArray<btCollisionShape*>& shapes, btDiscreteDynamicsWorld* world, const Eigen::Vector3f& position, const Eigen::Vector3f& size, float mass, float restitution);
	void AddCylinderShape(btAlignedObjectArray<btCollisionShape*>& shapes, btDiscreteDynamicsWorld* world, const Eigen::Vector3f& position, const Eigen::Vector3f& size, float mass, float restitution);
	Eigen::Matrix4f getRigidBodyTransform(btDynamicsWorld* world, int idx);

private:
};