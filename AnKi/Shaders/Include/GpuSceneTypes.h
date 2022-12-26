// Copyright (C) 2009-2022, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <AnKi/Shaders/Include/MeshTypes.h>

ANKI_BEGIN_NAMESPACE

/// @note All offsets in bytes
struct GpuSceneRenderable
{
	U32 m_worldTransformsOffset; ///< First is the crnt transform and the 2nd the previous
	U32 m_aabbOffset;
	U32 m_uniformsOffset;
	U32 m_geometryOffset;
	U32 m_boneTransformsOffset;
	U32 m_previousBoneTransformsOffset;
	U32 m_padding0;
	U32 m_padding1;
};
static_assert(sizeof(GpuSceneRenderable) == sizeof(Vec4) * 2);

struct GpuSceneMeshLod
{
	U32 m_vertexOffsets[(U32)VertexStreamId::kMeshRelatedCount];
	U32 m_indexCount;
	U32 m_indexOffset; // TODO Decide on its type
};
static_assert(sizeof(GpuSceneMeshLod) == sizeof(Vec4) * 2);

struct GpuSceneMesh
{
	GpuSceneMeshLod m_lods[kMaxLodCount];

	Vec3 m_positionTranslation;
	F32 m_positionScale;
};
static_assert(sizeof(GpuSceneMesh) == sizeof(Vec4) * (kMaxLodCount * 2 + 1));

struct GpuSceneParticles
{
	U32 m_vertexOffsets[(U32)VertexStreamId::kParticleRelatedCount];
	U32 m_padding0;
	U32 m_padding1;
};
static_assert(sizeof(GpuSceneParticles) == sizeof(Vec4) * 2);

struct UnpackedGpuSceneRenderableInstance
{
	U32 m_renderableOffset;
	U32 m_lod;
};

typedef U32 PackedGpuSceneRenderableInstance;

struct RenderableGpuView
{
	Mat3x4 m_worldTransform;
	Mat3x4 m_previousWorldTransform;
	Vec4 m_positionScaleF32AndTranslationVec3; ///< The scale and translation that uncompress positions.
};

ANKI_END_NAMESPACE
