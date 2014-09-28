// Copyright (C) 2014, Panagiotis Christopoulos Charitos.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include "anki/scene/Camera.h"

namespace anki {

//==============================================================================
// Camera                                                                      =
//==============================================================================

//==============================================================================
Camera::Camera(
	const CString& name, SceneGraph* scene, // SceneNode
	Type type, Frustum* frustum) 
:	SceneNode(name, scene), 
	MoveComponent(this),
	FrustumComponent(this, frustum),
	SpatialComponent(this),
	m_type(type)
{
	// Init components
	addComponent(static_cast<MoveComponent*>(this));
	addComponent(static_cast<FrustumComponent*>(this));
	addComponent(static_cast<SpatialComponent*>(this));
}

//==============================================================================
Camera::~Camera()
{}

//==============================================================================
void Camera::lookAtPoint(const Vec3& point)
{
	MoveComponent& move = *this;

	Vec4 j = Vec4(0.0, 1.0, 0.0, 0.0);
	Vec4 vdir = 
		(point.xyz0() - move.getLocalTransform().getOrigin()).getNormalized();
	Vec4 vup = j - vdir * j.dot(vdir);
	Vec4 vside = vdir.cross(vup);

	Mat3x4 rot = move.getLocalTransform().getRotation();
	rot.setColumns(vside.xyz(), vup.xyz(), (-vdir).xyz());
	move.setLocalRotation(rot);
}

//==============================================================================
void Camera::frustumUpdate()
{
	// Frustum (it's marked for update)
	FrustumComponent& fr = *this;
	fr.setProjectionMatrix(fr.getFrustum().calculateProjectionMatrix());
	fr.setViewProjectionMatrix(fr.getProjectionMatrix() * fr.getViewMatrix());

	// Spatial
	SpatialComponent& sp = *this;
	sp.markForUpdate();
}

//==============================================================================
void Camera::onMoveComponentUpdate(SceneNode&, F32, F32)
{
	// Frustum
	FrustumComponent& fr = *this;

	fr.setViewMatrix(Mat4(getWorldTransform().getInverse()));
	fr.setViewProjectionMatrix(fr.getProjectionMatrix() * fr.getViewMatrix());
	fr.getFrustum().resetTransform(getWorldTransform());

	// Spatial
	SpatialComponent& sp = *this;
	sp.markForUpdate();
}

//==============================================================================
// PerspectiveCamera                                                           =
//==============================================================================

//==============================================================================
PerspectiveCamera::PerspectiveCamera(const CString& name, SceneGraph* scene)
:	Camera(name, scene, Type::PERSPECTIVE, &m_frustum)
{}

//==============================================================================
// OrthographicCamera                                                          =
//==============================================================================

//==============================================================================
OrthographicCamera::OrthographicCamera(const CString& name, SceneGraph* scene)
:	Camera(name, scene, Type::ORTHOGRAPHIC, &m_frustum)
{}

} // end namespace anki
