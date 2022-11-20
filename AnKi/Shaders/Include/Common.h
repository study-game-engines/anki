// Copyright (C) 2009-2022, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

//! == C++ =============================================================================================================
#if defined(__cplusplus)

#	include <AnKi/Math.h>

#	define ANKI_BEGIN_NAMESPACE namespace anki {
#	define ANKI_END_NAMESPACE }
#	define ANKI_SHADER_FUNC_INLINE inline

#	define ANKI_SHADER_STATIC_ASSERT(cond_) static_assert(cond_)

ANKI_BEGIN_NAMESPACE

using Address = U64;

using ScalarVec4 = Array<F32, 4>;
using ScalarMat3x4 = Array<F32, 12>;
using ScalarMat4 = Array<F32, 16>;
ANKI_END_NAMESPACE

#	define ANKI_RP

//! == GLSL ============================================================================================================
#else
#	define ANKI_BEGIN_NAMESPACE
#	define ANKI_END_NAMESPACE
#	define ANKI_SHADER_FUNC_INLINE

#	define ANKI_SHADER_STATIC_ASSERT(cond_)

#	define ScalarVec4 Vec4
#	define ScalarMat3x4 Mat3x4
#	define ScalarMat4 Mat4

#	define constexpr const

#	define ANKI_SUPPORTS_64BIT !ANKI_PLATFORM_MOBILE

#	extension GL_EXT_control_flow_attributes : require
#	extension GL_KHR_shader_subgroup_vote : require
#	extension GL_KHR_shader_subgroup_ballot : require
#	extension GL_KHR_shader_subgroup_shuffle : require
#	extension GL_KHR_shader_subgroup_arithmetic : require

#	extension GL_EXT_samplerless_texture_functions : require
#	extension GL_EXT_shader_image_load_formatted : require
#	extension GL_EXT_nonuniform_qualifier : enable

#	extension GL_EXT_buffer_reference : enable
#	extension GL_EXT_buffer_reference2 : enable

#	extension GL_EXT_shader_explicit_arithmetic_types : enable
#	extension GL_EXT_shader_explicit_arithmetic_types_int8 : enable
#	extension GL_EXT_shader_explicit_arithmetic_types_int16 : enable
#	extension GL_EXT_shader_explicit_arithmetic_types_int32 : enable
#	extension GL_EXT_shader_explicit_arithmetic_types_float16 : enable
#	extension GL_EXT_shader_explicit_arithmetic_types_float32 : enable

#	if ANKI_SUPPORTS_64BIT
#		extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable
#		extension GL_EXT_shader_explicit_arithmetic_types_float64 : enable
#		extension GL_EXT_shader_atomic_int64 : enable
#		extension GL_EXT_shader_subgroup_extended_types_int64 : enable
#	endif

#	extension GL_EXT_nonuniform_qualifier : enable
#	extension GL_EXT_scalar_block_layout : enable

#	if defined(ANKI_RAY_GEN_SHADER) || defined(ANKI_ANY_HIT_SHADER) || defined(ANKI_CLOSEST_HIT_SHADER) \
		|| defined(ANKI_MISS_SHADER) || defined(ANKI_INTERSECTION_SHADER) || defined(ANKI_CALLABLE_SHADER)
#		extension GL_EXT_ray_tracing : enable
#	endif

#	define F32 float
const uint kSizeof_float = 4u;
#	define Vec2 vec2
const uint kSizeof_vec2 = 8u;
#	define Vec3 vec3
const uint kSizeof_vec3 = 12u;
#	define Vec4 vec4
const uint kSizeof_vec4 = 16u;

#	define F16 float16_t
const uint kSizeof_float16_t = 2u;
#	define HVec2 f16vec2
const uint kSizeof_f16vec2 = 4u;
#	define HVec3 f16vec3
const uint kSizeof_f16vec3 = 6u;
#	define HVec4 f16vec4
const uint kSizeof_f16vec4 = 8u;

#	define U8 uint8_t
const uint kSizeof_uint8_t = 1u;
#	define U8Vec2 u8vec2
const uint kSizeof_u8vec2 = 2u;
#	define U8Vec3 u8vec3
const uint kSizeof_u8vec3 = 3u;
#	define U8Vec4 u8vec4
const uint kSizeof_u8vec4 = 4u;

#	define I8 int8_t
const uint kSizeof_int8_t = 1u;
#	define I8Vec2 i8vec2
const uint kSizeof_i8vec2 = 2u;
#	define I8Vec3 i8vec3
const uint kSizeof_i8vec3 = 3u;
#	define I8Vec4 i8vec4
const uint kSizeof_i8vec4 = 4u;

#	define U16 uint16_t
const uint kSizeof_uint16_t = 2u;
#	define U16Vec2 u16vec2
const uint kSizeof_u16vec2 = 4u;
#	define U16Vec3 u16vec3
const uint kSizeof_u16vec3 = 6u;
#	define U16Vec4 u16vec4
const uint kSizeof_u16vec4 = 8u;

#	define I16 int16_t
const uint kSizeof_int16_t = 2u;
#	define I16Vec2 i16vec2
const uint kSizeof_i16vec2 = 4u;
#	define I16Vec3 i16vec3
const uint kSizeof_i16vec3 = 6u;
#	define i16Vec4 i16vec4
const uint kSizeof_i16vec4 = 8u;

#	define U32 uint
const uint kSizeof_uint = 4u;
#	define UVec2 uvec2
const uint kSizeof_uvec2 = 8u;
#	define UVec3 uvec3
const uint kSizeof_uvec3 = 12u;
#	define UVec4 uvec4
const uint kSizeof_uvec4 = 16u;

#	define I32 int
const uint kSizeof_int = 4u;
#	define IVec2 ivec2
const uint kSizeof_ivec2 = 8u;
#	define IVec3 ivec3
const uint kSizeof_ivec3 = 12u;
#	define IVec4 ivec4
const uint kSizeof_ivec4 = 16u;

#	if ANKI_SUPPORTS_64BIT
#		define U64 uint64_t
const uint kSizeof_uint64_t = 8u;
#		define U64Vec2 u64vec2
const uint kSizeof_u64vec2 = 16u;
#		define U64Vec3 u64vec3
const uint kSizeof_u64vec3 = 24u;
#		define U64Vec4 u64vec4
const uint kSizeof_u64vec4 = 32u;

#		define I64 int64_t
const uint kSizeof_int64_t = 8u;
#		define I64Vec2 i64vec2
const uint kSizeof_i64vec2 = 16u;
#		define I64Vec3 i64vec3
const uint kSizeof_i64vec3 = 24u;
#		define I64Vec4 i64vec4
const uint kSizeof_i64vec4 = 32u;
#	endif

#	define Mat3 mat3
const uint kSizeof_mat3 = 36u;

#	define Mat4 mat4
const uint kSizeof_mat4 = 64u;

#	define Mat3x4 mat4x3 // GLSL has the column number first and then the rows
const uint kSizeof_mat4x3 = 48u;

#	define Bool bool

#	if ANKI_SUPPORTS_64BIT
#		define Address U64
#	else
#		define Address UVec2
#	endif

#	define _ANKI_CONCATENATE(a, b) a##b
#	define ANKI_CONCATENATE(a, b) _ANKI_CONCATENATE(a, b)

#	define sizeof(type) _ANKI_CONCATENATE(kSizeof_, type)
#	define alignof(type) _ANKI_CONCATENATE(kAlignof_, type)

#	define _ANKI_SCONST_X(type, n, id) \
		layout(constant_id = id) const type n = type(1); \
		const U32 ANKI_CONCATENATE(n, _CONST_ID) = id

#	define _ANKI_SCONST_X2(type, componentType, n, id, constWorkaround) \
		layout(constant_id = id + 0u) const componentType ANKI_CONCATENATE(_anki_const_0_2_, n) = componentType(1); \
		layout(constant_id = id + 1u) const componentType ANKI_CONCATENATE(_anki_const_1_2_, n) = componentType(1); \
		constWorkaround type n = type(ANKI_CONCATENATE(_anki_const_0_2_, n), ANKI_CONCATENATE(_anki_const_1_2_, n))

#	define _ANKI_SCONST_X3(type, componentType, n, id, constWorkaround) \
		layout(constant_id = id + 0u) const componentType ANKI_CONCATENATE(_anki_const_0_3_, n) = componentType(1); \
		layout(constant_id = id + 1u) const componentType ANKI_CONCATENATE(_anki_const_1_3_, n) = componentType(1); \
		layout(constant_id = id + 2u) const componentType ANKI_CONCATENATE(_anki_const_2_3_, n) = componentType(1); \
		constWorkaround type n = type(ANKI_CONCATENATE(_anki_const_0_3_, n), ANKI_CONCATENATE(_anki_const_1_3_, n), \
									  ANKI_CONCATENATE(_anki_const_2_3_, n))

#	define _ANKI_SCONST_X4(type, componentType, n, id, constWorkaround) \
		layout(constant_id = id + 0u) const componentType ANKI_CONCATENATE(_anki_const_0_4_, n) = componentType(1); \
		layout(constant_id = id + 1u) const componentType ANKI_CONCATENATE(_anki_const_1_4_, n) = componentType(1); \
		layout(constant_id = id + 2u) const componentType ANKI_CONCATENATE(_anki_const_2_4_, n) = componentType(1); \
		layout(constant_id = id + 3u) const componentType ANKI_CONCATENATE(_anki_const_3_4_, n) = componentType(1); \
		constWorkaround type n = type(ANKI_CONCATENATE(_anki_const_0_4_, n), ANKI_CONCATENATE(_anki_const_1_4_, n), \
									  ANKI_CONCATENATE(_anki_const_2_4_, n), ANKI_CONCATENATE(_anki_const_2_4_, n))

#	define ANKI_SPECIALIZATION_CONSTANT_I32(n, id) _ANKI_SCONST_X(I32, n, id)
#	define ANKI_SPECIALIZATION_CONSTANT_IVEC2(n, id) _ANKI_SCONST_X2(IVec2, I32, n, id, const)
#	define ANKI_SPECIALIZATION_CONSTANT_IVEC3(n, id) _ANKI_SCONST_X3(IVec3, I32, n, id, const)
#	define ANKI_SPECIALIZATION_CONSTANT_IVEC4(n, id) _ANKI_SCONST_X4(IVec4, I32, n, id, const)

#	define ANKI_SPECIALIZATION_CONSTANT_U32(n, id) _ANKI_SCONST_X(U32, n, id)
#	define ANKI_SPECIALIZATION_CONSTANT_UVEC2(n, id) _ANKI_SCONST_X2(UVec2, U32, n, id, const)
#	define ANKI_SPECIALIZATION_CONSTANT_UVEC3(n, id) _ANKI_SCONST_X3(UVec3, U32, n, id, const)
#	define ANKI_SPECIALIZATION_CONSTANT_UVEC4(n, id) _ANKI_SCONST_X4(UVec4, U32, n, id, const)

#	define ANKI_SPECIALIZATION_CONSTANT_F32(n, id) _ANKI_SCONST_X(F32, n, id)
#	define ANKI_SPECIALIZATION_CONSTANT_VEC2(n, id) _ANKI_SCONST_X2(Vec2, F32, n, id, )
#	define ANKI_SPECIALIZATION_CONSTANT_VEC3(n, id) _ANKI_SCONST_X3(Vec3, F32, n, id, )
#	define ANKI_SPECIALIZATION_CONSTANT_VEC4(n, id) _ANKI_SCONST_X4(Vec4, F32, n, id, )

#	define ANKI_DEFINE_LOAD_STORE(type, alignment) \
		layout(buffer_reference, scalar, buffer_reference_align = (alignment)) buffer _Ref##type \
		{ \
			type m_value; \
		}; \
		void load(U64 address, out type o) \
		{ \
			o = _Ref##type(address).m_value; \
		} \
		void store(U64 address, type i) \
		{ \
			_Ref##type(address).m_value = i; \
		}

layout(std140, row_major) uniform;
layout(std140, row_major) buffer;

#	if ANKI_FORCE_FULL_FP_PRECISION
#		define ANKI_RP
#	else
#		define ANKI_RP mediump
#	endif

#	define ANKI_FP highp

precision highp int;
precision highp float;

#	define ANKI_BINDLESS_SET(s) \
		layout(set = s, binding = 0) uniform utexture2D u_bindlessTextures2dU32[kMaxBindlessTextures]; \
		layout(set = s, binding = 0) uniform itexture2D u_bindlessTextures2dI32[kMaxBindlessTextures]; \
		layout(set = s, binding = 0) uniform texture2D u_bindlessTextures2dF32[kMaxBindlessTextures]; \
		layout(set = s, binding = 0) uniform texture2DArray u_bindlessTextures2dArrayF32[kMaxBindlessTextures]; \
		layout(set = s, binding = 1) uniform textureBuffer u_bindlessTextureBuffers[kMaxBindlessReadonlyTextureBuffers];

Vec2 pow(Vec2 a, F32 b)
{
	return pow(a, Vec2(b));
}

Vec3 pow(Vec3 a, F32 b)
{
	return pow(a, Vec3(b));
}

Vec4 pow(Vec4 a, F32 b)
{
	return pow(a, Vec4(b));
}
#endif

//! == Consts ==========================================================================================================
ANKI_BEGIN_NAMESPACE

#if !defined(__cplusplus)
constexpr F32 kEpsilonf = 0.000001f;
constexpr F16 kEpsilonhf = 0.0001hf; // Divisions by this should be OK according to http://weitz.de/ieee/
constexpr ANKI_RP F32 kEpsilonRp = F32(kEpsilonhf);

constexpr U32 kMaxU32 = 0xFFFFFFFFu;
constexpr F32 kMaxF32 = 3.402823e+38;
constexpr F16 kMaxF16 = 65504.0hf;
constexpr F16 kMinF16 = 0.00006104hf;

constexpr F32 kPi = 3.14159265358979323846f;
#endif

/// The renderer will group drawcalls into instances up to this number.
constexpr U32 kMaxInstanceCount = 64u;

constexpr U32 kMaxLodCount = 3u;
constexpr U32 kMaxShadowCascades = 4u;

constexpr F32 kShadowsPolygonOffsetFactor = 1.25f;
constexpr F32 kShadowsPolygonOffsetUnits = 2.75f;

ANKI_END_NAMESPACE
