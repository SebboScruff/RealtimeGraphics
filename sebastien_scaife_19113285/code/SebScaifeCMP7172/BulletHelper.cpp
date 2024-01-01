#include "BulletHelper.h"

void BulletHelper::AddBoxShape(btAlignedObjectArray<btCollisionShape*>& shapes, btDiscreteDynamicsWorld* world, const Eigen::Vector3f& position, const Eigen::Vector3f& size, float mass, float restitution, btVector3 _localIntertia)
{
	btCollisionShape* shape = new btBoxShape(btVector3(size.x() * 0.5f, size.y() * 0.5f, size.z() * 0.5f));
	shapes.push_back(shape);
	btTransform transform;
	transform.setIdentity();
	transform.setOrigin(btVector3(position.x(), position.y(), position.z()));
	if (mass >= 0.f) {
		shape->calculateLocalInertia(mass, _localIntertia);
	}
	btDefaultMotionState* motionState = new btDefaultMotionState(transform);
	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState, shape, _localIntertia);
	rbInfo.m_restitution = restitution;
	btRigidBody* body = new btRigidBody(rbInfo);
	world->addRigidBody(body);
}

void BulletHelper::AddCylinderShape(btAlignedObjectArray<btCollisionShape*>& shapes, btDiscreteDynamicsWorld* world, const Eigen::Vector3f& position, const Eigen::Vector3f& size, float mass, float restitution, btVector3 _localIntertia)
{
	btCollisionShape* shape = new btCylinderShape(btVector3(size.x() * 0.5f, size.y() * 0.5f, size.z() * 0.5f));
	shapes.push_back(shape);
	btTransform transform;
	transform.setIdentity();
	transform.setOrigin(btVector3(position.x(), position.y(), position.z()));
	if (mass >= 0.f) {
		shape->calculateLocalInertia(mass, _localIntertia);
	}
	btDefaultMotionState* motionState = new btDefaultMotionState(transform);
	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState, shape, _localIntertia);
	rbInfo.m_restitution = restitution;
	btRigidBody* body = new btRigidBody(rbInfo);
	world->addRigidBody(body);
}

Eigen::Matrix4f BulletHelper::getRigidBodyTransform(btDynamicsWorld* world, int idx)
{
	btCollisionObject* obj = world->getCollisionObjectArray()[idx];
	btRigidBody* rb = btRigidBody::upcast(obj);
	btTransform transform;
	if (rb && rb->getMotionState()) {
		rb->getMotionState()->getWorldTransform(transform);
	}
	else {
		transform = obj->getWorldTransform();
	}
	Eigen::Quaternionf rot(transform.getRotation().w(), transform.getRotation().x(), transform.getRotation().y(), transform.getRotation().z());
	Eigen::Matrix4f rotMat = Eigen::Matrix4f::Identity();
	rotMat.block<3, 3>(0, 0) = rot.matrix();
	return makeTranslationMatrix(Eigen::Vector3f(transform.getOrigin().x(), transform.getOrigin().y(), transform.getOrigin().z())) * rotMat;
}