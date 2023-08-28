// Copyright (C) 2009-2023, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

// This file contains the GPU scene data types.
// These types have in-place initialization for some members. Members that are used in visibility testing. These default values aim to essentially
// make these objects not visible.

#pragma once

#include <AnKi/Shaders/Include/MeshTypes.h>

ANKI_BEGIN_NAMESPACE

/// Some far distance that will make objects not visible. Don't use kMaxF32 because this value will be used in math ops and it might overflow.
constexpr F32 kSomeFarDistance = 100000.0f;

/// @note All offsets in bytes
struct GpuSceneRenderable
{
	U32 m_worldTransformsOffset; ///< First is the crnt transform and the 2nd the previous
	U32 m_uniformsOffset;
	U32 m_meshLodsOffset; ///< Points to an array of GpuSceneMeshLod sized kMaxLodCount.
	U32 m_boneTransformsOffset; ///< Array of Mat3x4 or 0 if its not a skin.
	U32 m_particleEmitterOffset; ///< Offset to GpuSceneParticleEmitter or 0 if it's not an emitter.
	U32 m_rtShadowsShaderHandleIndex; ///< The index of the shader handle in the array of library's handles.
	U32 m_uuid;
};

/// Almost similar to GpuSceneRenderable but with only what the material shaders need. Needs to fit in a UVec4 vertex attribute.
struct GpuSceneRenderableVertex
{
	U32 m_worldTransformsOffset;
	U32 m_uniformsOffset;
	U32 m_meshLodOffset; ///< Points to a single GpuSceneMeshLod and not an array
	U32 m_boneTransformsOrParticleEmitterOffset;
};
static_assert(sizeof(GpuSceneRenderableVertex) == sizeof(UVec4));

/// Used in visibility testing.
struct GpuSceneRenderableAabb
{
	Vec3 m_sphereCenter ANKI_CPP_CODE(= Vec3(kSomeFarDistance));
	F32 m_sphereRadius ANKI_CPP_CODE(= 0.0f);

	Vec3 m_aabbExtend ANKI_CPP_CODE(= Vec3(0.0f));
	U32 m_renderableIndexAndRenderStateBucket; ///< High 20bits point to a GpuSceneRenderable. Rest 12bits are the render state bucket idx.
};
static_assert(sizeof(GpuSceneRenderableAabb) == sizeof(Vec4) * 2);

/// Represents the geometry data of a single LOD of an indexed mesh.
struct GpuSceneMeshLod
{
	U32 m_vertexOffsets[(U32)VertexStreamId::kMeshRelatedCount];
	U32 m_indexCount;
	U32 m_firstIndex; // In sizeof(indexType)

	Vec3 m_positionTranslation;
	F32 m_positionScale;

	UVec2 m_blasAddress;
	U32 m_tlasInstanceMask; ///< Mask that goes to AccelerationStructureInstance::m_instanceCustomIndex24_mask8
	U32 m_padding;
};
static_assert(sizeof(GpuSceneMeshLod) == sizeof(Vec4) * 4);

struct GpuSceneParticleEmitter
{
	U32 m_vertexOffsets[(U32)VertexStreamId::kParticleRelatedCount];
	U32 m_aliveParticleCount;
};
static_assert(sizeof(GpuSceneParticleEmitter) == sizeof(Vec4) * 2);

enum class GpuSceneLightFlag : U32
{
	kNone = 0,
	kPointLight = 1 << 0,
	kSpotLight = 1 << 1,
	kShadow = 1 << 2
};
ANKI_ENUM_ALLOW_NUMERIC_OPERATIONS(GpuSceneLightFlag)

/// Point or spot light.
struct GpuSceneLight
{
	Vec3 m_position ANKI_CPP_CODE(= Vec3(kSomeFarDistance)); ///< Position in world space.
	RF32 m_radius ANKI_CPP_CODE(= 0.0f); ///< Radius.

	RVec3 m_diffuseColor;
	RF32 m_squareRadiusOverOne; ///< 1/(radius^2).

	GpuSceneLightFlag m_flags;
	U32 m_arrayIndex; ///< Array index of the LightComponent in the CPU scene.
	U32 m_uuid; ///< The UUID of that light. If it's zero the GPU will not inform the CPU about it.
	F32 m_innerCos; ///< Only for spot light.

	RVec3 m_direction; ///< Only for spot light. Light direction.
	RF32 m_outerCos; ///< Only for spot light.

	Vec4 m_edgePoints[4u]; ///< Edge points in world space. Only for spot light.

	/// If it's a spot light the 4 first rows are the texture matrix. If it's point light it's the UV viewports in the shadow atlas.
	Vec4 m_spotLightMatrixOrPointLightUvViewports[6u];
};

/// Representation of a reflection probe.
struct GpuSceneReflectionProbe
{
	Vec3 m_position; ///< Position of the probe in world space.
	U32 m_cubeTexture; ///< Bindless index of the reflection texture.

	Vec3 m_aabbMin ANKI_CPP_CODE(= Vec3(kSomeFarDistance));
	U32 m_uuid;

	Vec3 m_aabbMax ANKI_CPP_CODE(= Vec3(kSomeFarDistance));
	U32 m_arrayIndex; ///< Array in the CPU scene.
};
constexpr U32 kSizeof_GpuSceneReflectionProbe = 3u * sizeof(Vec4);
static_assert(sizeof(GpuSceneReflectionProbe) == kSizeof_GpuSceneReflectionProbe);

/// Global illumination probe
struct GpuSceneGlobalIlluminationProbe
{
	Vec3 m_aabbMin ANKI_CPP_CODE(= Vec3(kSomeFarDistance));
	U32 m_uuid;

	Vec3 m_aabbMax ANKI_CPP_CODE(= Vec3(kSomeFarDistance));
	U32 m_arrayIndex; ///< Array in the CPU scene.

	U32 m_volumeTexture; ///< Bindless index of the irradiance volume texture.
	F32 m_halfTexelSizeU; ///< (1.0 / textureSize(texArr[textureIndex]).x) / 2.0
	RF32 m_fadeDistance; ///< Used to calculate a factor that is zero when fragPos is close to AABB bounds and 1.0 at fadeDistance and less.
	F32 m_padding2;
};
constexpr U32 kSizeof_GpuSceneGlobalIlluminationProbe = 3u * sizeof(Vec4);
static_assert(sizeof(GpuSceneGlobalIlluminationProbe) == kSizeof_GpuSceneGlobalIlluminationProbe);

/// Decal.
struct GpuSceneDecal
{
	U32 m_diffuseTexture;
	U32 m_roughnessMetalnessTexture;
	RF32 m_diffuseBlendFactor;
	RF32 m_roughnessMetalnessFactor;

	Mat4 m_textureMatrix;

	Vec3 m_sphereCenter ANKI_CPP_CODE(= Vec3(kSomeFarDistance));
	F32 m_sphereRadius ANKI_CPP_CODE(= 0.0f);
};
constexpr U32 kSizeof_GpuSceneDecal = 2u * sizeof(Vec4) + 1u * sizeof(Mat4);
static_assert(sizeof(GpuSceneDecal) == kSizeof_GpuSceneDecal);

/// Fog density volume.
struct GpuSceneFogDensityVolume
{
	Vec3 m_aabbMinOrSphereCenter ANKI_CPP_CODE(= Vec3(kSomeFarDistance));
	U32 m_isBox ANKI_CPP_CODE(= 1);

	Vec3 m_aabbMaxOrSphereRadius ANKI_CPP_CODE(= Vec3(kSomeFarDistance));
	RF32 m_density;
};
constexpr U32 kSizeof_GpuSceneFogDensityVolume = 2u * sizeof(Vec4);
static_assert(sizeof(GpuSceneFogDensityVolume) == kSizeof_GpuSceneFogDensityVolume);

enum class GpuSceneNonRenderableObjectType : U32
{
	kLight,
	kDecal,
	kFogDensityVolume,
	kReflectionProbe,
	kGlobalIlluminationProbe,

	kCount,
	kFirst = 0
};
ANKI_ENUM_ALLOW_NUMERIC_OPERATIONS(GpuSceneNonRenderableObjectType)

#define ANKI_GPU_SCENE_NON_RENDERABLE_OBJECT_TYPE_LIGHT 0
#define ANKI_GPU_SCENE_NON_RENDERABLE_OBJECT_TYPE_DECAL 1
#define ANKI_GPU_SCENE_NON_RENDERABLE_OBJECT_TYPE_FOG_DENSITY_VOLUME 2
#define ANKI_GPU_SCENE_NON_RENDERABLE_OBJECT_TYPE_REFLECTION_PROBE 3
#define ANKI_GPU_SCENE_NON_RENDERABLE_OBJECT_TYPE_GLOBAL_ILLUMINATION_PROBE 4
#define ANKI_GPU_SCENE_NON_RENDERABLE_OBJECT_TYPE_COUNT 5

enum class GpuSceneNonRenderableObjectTypeBit : U32
{
	kNone = 0,

	kPLight = 1 << 0,
	kDecal = 1 << 1,
	kFogDensityVolume = 1 << 2,
	kReflectionProbe = 1 << 3,
	kGlobalIlluminationProbe = 1 << 4,
};
ANKI_ENUM_ALLOW_NUMERIC_OPERATIONS(GpuSceneNonRenderableObjectTypeBit)

/// Non-renderable types that require GPU to CPU feedback.
enum class GpuSceneNonRenderableObjectTypeWithFeedback : U32
{
	kLight,
	kReflectionProbe,
	kGlobalIlluminationProbe,

	kCount,
	kFirst = 0
};
ANKI_ENUM_ALLOW_NUMERIC_OPERATIONS(GpuSceneNonRenderableObjectTypeWithFeedback)

ANKI_END_NAMESPACE
