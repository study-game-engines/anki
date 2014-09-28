// Copyright (C) 2014, Panagiotis Christopoulos Charitos.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#ifndef ANKI_SCENE_STATIC_GEOMETRY_NODE_H
#define ANKI_SCENE_STATIC_GEOMETRY_NODE_H

#include "anki/scene/Common.h"
#include "anki/scene/SceneNode.h"
#include "anki/scene/SpatialComponent.h"
#include "anki/scene/RenderComponent.h"

namespace anki {

/// @addtogroup scene
/// @{

/// Part of the static geometry. Used only for visibility tests
class StaticGeometrySpatial: public SpatialComponent
{
public:
	StaticGeometrySpatial(SceneNode* node, const Obb* obb);

	const CollisionShape& getSpatialCollisionShape()
	{
		return *m_obb;
	}

	Vec4 getSpatialOrigin()
	{
		return m_obb->getCenter();
	}

private:
	const Obb* m_obb;
};

/// Static geometry scene node patch
class StaticGeometryPatchNode: public SceneNode, public SpatialComponent,
	public RenderComponent
{
public:
	StaticGeometryPatchNode(
		const CString& name, SceneGraph* scene, // Scene
		const ModelPatchBase* modelPatch); // Self

	~StaticGeometryPatchNode();

	/// @name SpatialComponent virtuals
	/// @{
	const CollisionShape& getSpatialCollisionShape()
	{
		return *m_obb;
	}

	Vec4 getSpatialOrigin()
	{
		return m_obb->getCenter();
	}
	/// @}

	/// @name RenderComponent virtuals
	/// @{
	void buildRendering(RenderingBuildData& data);

	const Material& getMaterial()
	{
		return m_modelPatch->getMaterial();
	}
	/// @}

private:
	const ModelPatchBase* m_modelPatch;
	const Obb* m_obb; 
};

/// Static geometry scene node
class StaticGeometryNode: public SceneNode
{
public:
	StaticGeometryNode(
		const CString& name, SceneGraph* scene, // Scene
		const CString& filename); // Self

	~StaticGeometryNode();

private:
	ModelResourcePointer m_model;
};

/// @}

} // end namespace anki

#endif
