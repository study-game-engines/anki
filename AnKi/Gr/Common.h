// Copyright (C) 2009-2022, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <AnKi/Util/Allocator.h>
#include <AnKi/Util/Ptr.h>
#include <AnKi/Util/String.h>
#include <AnKi/Util/Enum.h>

namespace anki {

// Forward
class GrObject;

class GrManager;
class GrManagerImpl;
class TextureInitInfo;
class TextureViewInitInfo;
class SamplerInitInfo;
class GrManagerInitInfo;
class FramebufferInitInfo;
class BufferInitInfo;
class ShaderInitInfo;
class ShaderProgramInitInfo;
class CommandBufferInitInfo;
class AccelerationStructureInitInfo;
class GrUpscalerInitInfo;

/// @addtogroup graphics
/// @{

#define ANKI_GR_LOGI(...) ANKI_LOG("GR", NORMAL, __VA_ARGS__)
#define ANKI_GR_LOGE(...) ANKI_LOG("GR", ERROR, __VA_ARGS__)
#define ANKI_GR_LOGW(...) ANKI_LOG("GR", WARNING, __VA_ARGS__)
#define ANKI_GR_LOGF(...) ANKI_LOG("GR", FATAL, __VA_ARGS__)

// Some constants
constexpr U32 MAX_VERTEX_ATTRIBUTES = 8;
constexpr U32 MAX_COLOR_ATTACHMENTS = 4;
constexpr U32 MAX_DESCRIPTOR_SETS = 3; ///< Groups that can be bound at the same time.
constexpr U32 MAX_BINDINGS_PER_DESCRIPTOR_SET = 32;
constexpr U32 MAX_FRAMES_IN_FLIGHT = 3; ///< Triple buffering.
constexpr U32 MAX_GR_OBJECT_NAME_LENGTH = 31;
constexpr U32 MAX_BINDLESS_TEXTURES = 512;
constexpr U32 MAX_BINDLESS_READONLY_TEXTURE_BUFFERS = 512;

/// The number of commands in a command buffer that make it a small batch command buffer.
constexpr U32 COMMAND_BUFFER_SMALL_BATCH_MAX_COMMANDS = 100;

/// Smart pointer for resources.
template<typename T>
using GrObjectPtrT = IntrusivePtr<T, DefaultPtrDeleter<GrObject>>;

using GrObjectPtr = GrObjectPtrT<GrObject>;

#define ANKI_GR_CLASS(x_) \
	class x_##Impl; \
	class x_; \
	using x_##Ptr = GrObjectPtrT<x_>;

ANKI_GR_CLASS(Buffer)
ANKI_GR_CLASS(Texture)
ANKI_GR_CLASS(TextureView)
ANKI_GR_CLASS(Sampler)
ANKI_GR_CLASS(CommandBuffer)
ANKI_GR_CLASS(Shader)
ANKI_GR_CLASS(Framebuffer)
ANKI_GR_CLASS(OcclusionQuery)
ANKI_GR_CLASS(TimestampQuery)
ANKI_GR_CLASS(ShaderProgram)
ANKI_GR_CLASS(Fence)
ANKI_GR_CLASS(RenderGraph)
ANKI_GR_CLASS(AccelerationStructure)
ANKI_GR_CLASS(GrUpscaler)

#undef ANKI_GR_CLASS

#define ANKI_GR_OBJECT \
	friend class GrManager; \
	template<typename, typename> \
	friend class IntrusivePtr; \
	template<typename, typename> \
	friend class GenericPoolAllocator;

/// Shader block information.
class ShaderVariableBlockInfo
{
public:
	I16 m_offset = -1; ///< Offset inside the block

	I16 m_arraySize = -1; ///< Number of elements.

	/// Stride between the each array element if the variable is array.
	I16 m_arrayStride = -1;

	/// Identifying the stride between columns of a column-major matrix or rows of a row-major matrix.
	I16 m_matrixStride = -1;
};

/// Knowing the vendor allows some optimizations
enum class GpuVendor : U8
{
	kUnknown,
	kArm,
	kNvidia,
	kAMD,
	kIntel,
	kQualcomm,
	kCount
};

inline constexpr Array<CString, U(GpuVendor::kCount)> GPU_VENDOR_STR = {"unknown", "ARM",   "nVidia",
																		"AMD",     "Intel", "Qualcomm"};

/// Device capabilities.
ANKI_BEGIN_PACKED_STRUCT
class GpuDeviceCapabilities
{
public:
	/// The alignment of offsets when bounding uniform buffers.
	U32 m_uniformBufferBindOffsetAlignment = MAX_U32;

	/// The max visible range of uniform buffers inside the shaders.
	PtrSize m_uniformBufferMaxRange = 0;

	/// The alignment of offsets when bounding storage buffers.
	U32 m_storageBufferBindOffsetAlignment = MAX_U32;

	/// The max visible range of storage buffers inside the shaders.
	PtrSize m_storageBufferMaxRange = 0;

	/// The alignment of offsets when bounding texture buffers.
	U32 m_textureBufferBindOffsetAlignment = MAX_U32;

	/// The max visible range of texture buffers inside the shaders.
	PtrSize m_textureBufferMaxRange = 0;

	/// Max push constant size.
	PtrSize m_pushConstantsSize = 128;

	/// The max combined size of shared variables (with paddings) in compute shaders.
	PtrSize m_computeSharedMemorySize = 16_KB;

	/// Each SBT record should be a multiple of this.
	U32 m_sbtRecordAlignment = MAX_U32;

	/// The size of a shader group handle that will be placed inside an SBT record.
	U32 m_shaderGroupHandleSize = 0;

	/// Min subgroup size of the GPU.
	U32 m_minSubgroupSize = 0;

	/// Max subgroup size of the GPU.
	U32 m_maxSubgroupSize = 0;

	/// Min size of a texel in the shading rate image.
	U32 m_minShadingRateImageTexelSize = 0;

	/// GPU vendor.
	GpuVendor m_gpuVendor = GpuVendor::kUnknown;

	/// Descrete or integrated GPU.
	Bool m_discreteGpu = false;

	/// API version.
	U8 m_minorApiVersion = 0;

	/// API version.
	U8 m_majorApiVersion = 0;

	/// RT.
	Bool m_rayTracingEnabled = false;

	/// 64 bit atomics.
	Bool m_64bitAtomics = false;

	/// VRS.
	Bool m_vrs = false;

	/// Supports min/max texture filtering.
	Bool m_samplingFilterMinMax = false;

	/// Supports or not 24bit, 48bit or 96bit texture formats.
	Bool m_unalignedBbpTextureFormats = false;

	/// DLSS.
	Bool m_dlss = false;
};
ANKI_END_PACKED_STRUCT

/// The type of the allocator for heap allocations
template<typename T>
using GrAllocator = HeapAllocator<T>;

/// The base of all init infos for GR.
class GrBaseInitInfo
{
public:
	/// @name The name of the object.
	GrBaseInitInfo(CString name)
	{
		setName(name);
	}

	GrBaseInitInfo()
		: GrBaseInitInfo(CString())
	{
	}

	GrBaseInitInfo(const GrBaseInitInfo& b)
	{
		m_name = b.m_name;
	}

	GrBaseInitInfo& operator=(const GrBaseInitInfo& b)
	{
		m_name = b.m_name;
		return *this;
	}

	CString getName() const
	{
		return (m_name[0] != '\0') ? CString(&m_name[0]) : CString();
	}

	void setName(CString name)
	{
		// Zero it because the derived classes may be hashed.
		zeroMemory(m_name);

		U32 len;
		if(name && (len = name.getLength()) > 0)
		{
			len = min(len, MAX_GR_OBJECT_NAME_LENGTH);
			memcpy(&m_name[0], &name[0], len);
		}
	}

private:
	Array<char, MAX_GR_OBJECT_NAME_LENGTH + 1> m_name;
};

enum class ColorBit : U8
{
	kNone = 0,
	kRed = 1 << 0,
	kGreen = 1 << 1,
	kBlue = 1 << 2,
	kAlpha = 1 << 3,
	kAll = kRed | kGreen | kBlue | kAlpha
};
ANKI_ENUM_ALLOW_NUMERIC_OPERATIONS(ColorBit)

enum class PrimitiveTopology : U8
{
	kPoints,
	kLines,
	kLineStip,
	kTriangles,
	kTriangleStrip,
	kPatchs
};

enum class FillMode : U8
{
	kPoints,
	kWireframe,
	kSolid,
	kCount
};

enum class FaceSelectionBit : U8
{
	kNone = 0,
	kFront = 1 << 0,
	kBack = 1 << 1,
	kFrontAndBack = kFront | kBack
};
ANKI_ENUM_ALLOW_NUMERIC_OPERATIONS(FaceSelectionBit)

enum class CompareOperation : U8
{
	kAlways,
	kLess,
	kEqual,
	kLessEqual,
	kGreater,
	kGreaterEqual,
	kNotEqual,
	kNever,
	kCount
};

enum class StencilOperation : U8
{
	kKeep,
	kZero,
	kReplace,
	kIncrementAndClamp,
	kDecrementAndClamp,
	kInvert,
	kIncrementAndWrap,
	kDecrementAndWrap,
	kCount
};

enum class BlendFactor : U8
{
	kZero,
	kOne,
	kSrcColor,
	kOneMinusSrcColor,
	kDstColor,
	kOneMinusDstColor,
	kSrcAlpha,
	kOneMinusSrcAlpha,
	kDstAlpha,
	kOneMinusDstAlpha,
	kConstantColor,
	kOneMinusConstantColor,
	kConstantAlpha,
	kOneMinusConstantAlpha,
	kSrcAlphaSaturate,
	kSrc1Color,
	kOneMinusSrc1Color,
	kSrc1Alpha,
	kOneMinusSrc1Alpha,
	kCount
};

enum class BlendOperation : U8
{
	kAdd,
	kSubtract,
	kReverseSubtract,
	kMin,
	kMax,
	kCount
};

enum class VertexStepRate : U8
{
	kVertex,
	kInstance,
	kDraw,
	kCount
};

/// A way to distinguish the aspect of a depth stencil texture.
enum class DepthStencilAspectBit : U8
{
	kNone = 0,
	kDepth = 1 << 0,
	kStencil = 1 << 1,
	kDepthStencil = kDepth | kStencil
};
ANKI_ENUM_ALLOW_NUMERIC_OPERATIONS(DepthStencilAspectBit)

/// Pixel or vertex format.
/// WARNING: Keep it the same as vulkan (one conversion less).
enum class Format : U32
{
	kNone = 0,

#define ANKI_FORMAT_DEF(type, id, componentCount, texelSize, blockWidth, blockHeight, blockSize, shaderType, \
						depthStencil) \
	k##type = id,
#include <AnKi/Gr/Format.defs.h>
#undef ANKI_FORMAT_DEF
};
ANKI_ENUM_ALLOW_NUMERIC_OPERATIONS(Format)

/// Contains info for a specific Format.
class FormatInfo
{
public:
	U8 m_componentCount; ///< The number of components.
	U8 m_texelSize; ///< The size of the texel. Only for incompressed, zero for compressed.
	U8 m_blockWidth; ///< The width of the block size of compressed formats. Zero otherwise.
	U8 m_blockHeight; ///< The height of the block size of compressed formats. Zero otherwise.
	U8 m_blockSize; ///< The size of the block of a compressed format. Zero otherwise.
	U8 m_shaderType; ///< It's 0 if the shader sees it as float, 1 if uint and 2 if signed int.
	DepthStencilAspectBit m_depthStencil; ///< Depth/stencil mask.
	const char* m_name;

	Bool isDepthStencil() const
	{
		return m_depthStencil != DepthStencilAspectBit::kNone;
	}

	Bool isDepth() const
	{
		return !!(m_depthStencil & DepthStencilAspectBit::kDepth);
	}

	Bool isStencil() const
	{
		return !!(m_depthStencil & DepthStencilAspectBit::kStencil);
	}

	Bool isCompressed() const
	{
		return m_blockSize > 0;
	}
};

/// Get info for a specific Format.
ANKI_PURE FormatInfo getFormatInfo(Format fmt);

/// Compute the size in bytes of a texture surface surface.
ANKI_PURE PtrSize computeSurfaceSize(U32 width, U32 height, Format fmt);

/// Compute the size in bytes of the texture volume.
ANKI_PURE PtrSize computeVolumeSize(U32 width, U32 height, U32 depth, Format fmt);

/// Texture type.
enum class TextureType : U8
{
	k1D,
	k2D,
	k3D,
	k2DArray,
	kCube,
	kCubeArray,
	kCount
};

inline Bool textureTypeIsCube(const TextureType t)
{
	return t == TextureType::kCube || t == TextureType::kCubeArray;
}

/// Texture usage hints. They are very important.
enum class TextureUsageBit : U32
{
	kNone = 0,

	kSampledGeometry = 1 << 0,
	kSampledFragment = 1 << 1,
	kSampledCompute = 1 << 2,
	kSampledTraceRays = 1 << 3,

	kImageGeometryRead = 1 << 4,
	kImageGeometryWrite = 1 << 5,
	kImageFragmentRead = 1 << 6,
	kImageFragmentWrite = 1 << 7,
	kImageComputeRead = 1 << 8,
	kImageComputeWrite = 1 << 9,
	kImageTraceRaysRead = 1 << 10,
	kImageTraceRaysWrite = 1 << 11,

	kFramebufferRead = 1 << 12,
	kFramebufferWrite = 1 << 13,
	kFramebufferShadingRate = 1 << 14,

	kTransferDestination = 1 << 15,
	kGenerateMipmaps = 1 << 16,

	kPresent = 1 << 17,

	// Derived
	kAllSampled = kSampledGeometry | kSampledFragment | kSampledCompute | kSampledTraceRays,
	kAllImage = kImageGeometryRead | kImageGeometryWrite | kImageFragmentRead | kImageFragmentWrite | kImageComputeRead
				| kImageComputeWrite | kImageTraceRaysRead | kImageTraceRaysWrite,
	kAllFramebuffer = kFramebufferRead | kFramebufferWrite,

	kAllGraphics = kSampledGeometry | kSampledFragment | kImageGeometryRead | kImageGeometryWrite | kImageFragmentRead
				   | kImageFragmentWrite | kFramebufferRead | kFramebufferWrite | kFramebufferShadingRate,
	kAllCompute = kSampledCompute | kImageComputeRead | kImageComputeWrite,
	kAllTransfer = kTransferDestination | kGenerateMipmaps,

	kAllRead = kAllSampled | kImageGeometryRead | kImageFragmentRead | kImageComputeRead | kImageTraceRaysRead
			   | kFramebufferRead | kFramebufferShadingRate | kPresent | kGenerateMipmaps,
	kAllWrite = kImageGeometryWrite | kImageFragmentWrite | kImageComputeWrite | kImageTraceRaysWrite
				| kFramebufferWrite | kTransferDestination | kGenerateMipmaps,
};
ANKI_ENUM_ALLOW_NUMERIC_OPERATIONS(TextureUsageBit)

enum class SamplingFilter : U8
{
	kNearest,
	kLinear,
	kMin, ///< It calculates the min of a 2x2 quad. Only if GpuDeviceCapabilities::m_samplingFilterMinMax is supported.
	kMax, ///< It calculates the max of a 2x2 quad. Only if GpuDeviceCapabilities::m_samplingFilterMinMax is supported.
	kBase ///< Only for mipmaps.
};

enum class SamplingAddressing : U8
{
	kClamp,
	kRepeat,
	kBlack,
	kWhite,

	kCount,
	kFirst = 0,
	kLast = kCount - 1,
};

enum class ShaderType : U16
{
	kVertex,
	kTessellationControl,
	kTessellationEvaluation,
	kGeometry,
	kFragment,
	kCompute,
	kRayGen,
	kAnyHit,
	kClosestHit,
	kMiss,
	kIntersection,
	kCallable,

	kCount,
	kFirst = 0,
	kLast = kCount - 1,
	kFirstGraphics = kVertex,
	kLastGraphics = kFragment,
	kFirstRayTracing = kRayGen,
	kLastRayTracing = kCallable,
};
ANKI_ENUM_ALLOW_NUMERIC_OPERATIONS(ShaderType)

enum class ShaderTypeBit : U16
{
	kVertex = 1 << 0,
	kTessellationControl = 1 << 1,
	kTessellationEvaluation = 1 << 2,
	kGeometry = 1 << 3,
	kFragment = 1 << 4,
	kCompute = 1 << 5,
	kRayGen = 1 << 6,
	kAnyHit = 1 << 7,
	kClosestHit = 1 << 8,
	kMiss = 1 << 9,
	kIntersection = 1 << 10,
	kCallable = 1 << 11,

	kNone = 0,
	kAllGraphics = kVertex | kTessellationControl | kTessellationEvaluation | kGeometry | kFragment,
	kAllRayTracing = kRayGen | kAnyHit | kClosestHit | kMiss | kIntersection | kCallable,
	kAll = kAllGraphics | kCompute | kAllRayTracing,
};
ANKI_ENUM_ALLOW_NUMERIC_OPERATIONS(ShaderTypeBit)

enum class ShaderVariableDataType : U8
{
	NONE,

#define ANKI_SVDT_MACRO(capital, type, baseType, rowCount, columnCount, isIntagralType) capital,
#define ANKI_SVDT_MACRO_OPAQUE(capital, type) capital,
#include <AnKi/Gr/ShaderVariableDataType.defs.h>
#undef ANKI_SVDT_MACRO
#undef ANKI_SVDT_MACRO_OPAQUE

	// Derived
	COUNT,

	NUMERICS_FIRST = I32,
	NUMERICS_LAST = MAT4,

	NUMERIC_1_COMPONENT_FIRST = I32,
	NUMERIC_1_COMPONENT_LAST = F32,
	NUMERIC_2_COMPONENT_FIRST = IVEC2,
	NUMERIC_2_COMPONENT_LAST = VEC2,
	NUMERIC_3_COMPONENT_FIRST = IVEC3,
	NUMERIC_3_COMPONENT_LAST = VEC3,
	NUMERIC_4_COMPONENT_FIRST = IVEC4,
	NUMERIC_4_COMPONENT_LAST = VEC4,

	MATRIX_FIRST = MAT3,
	MATRIX_LAST = MAT4,

	TEXTURE_FIRST = TEXTURE_1D,
	TEXTURE_LAST = TEXTURE_CUBE_ARRAY,

	IMAGE_FIRST = IMAGE_1D,
	IMAGE_LAST = IMAGE_CUBE_ARRAY,
};
ANKI_ENUM_ALLOW_NUMERIC_OPERATIONS(ShaderVariableDataType)

class ShaderVariableDataTypeInfo
{
public:
	const Char* m_name;
	U32 m_size; ///< Size of the type.
	Bool m_opaque;
	Bool m_isIntegral; ///< If true is integral type. Else it's float.
};

ANKI_PURE const ShaderVariableDataTypeInfo& getShaderVariableDataTypeInfo(ShaderVariableDataType type);

/// Occlusion query result bit.
enum class OcclusionQueryResultBit : U8
{
	NOT_AVAILABLE = 1 << 0,
	VISIBLE = 1 << 1,
	NOT_VISIBLE = 1 << 2
};
ANKI_ENUM_ALLOW_NUMERIC_OPERATIONS(OcclusionQueryResultBit)

/// Occlusion query result.
enum class OcclusionQueryResult : U8
{
	NOT_AVAILABLE,
	VISIBLE,
	NOT_VISIBLE
};

/// Timestamp query result.
enum class TimestampQueryResult : U8
{
	NOT_AVAILABLE,
	AVAILABLE
};

/// Attachment load operation.
enum class AttachmentLoadOperation : U8
{
	LOAD,
	CLEAR,
	DONT_CARE
};

/// Attachment store operation.
enum class AttachmentStoreOperation : U8
{
	STORE,
	DONT_CARE
};

/// Buffer usage modes.
/// The graphics work consists of the following pipes: INDIRECT, GEOMETRY (all programmable and fixed function geometry
/// stages) and finaly FRAGMENT.
/// The compute from the consists of the following: INDIRECT and COMPUTE.
/// The trace rays from the: INDIRECT and TRACE_RAYS
/// !!WARNING!! If you change this remember to change PrivateBufferUsageBit.
enum class BufferUsageBit : U64
{
	NONE = 0,

	UNIFORM_GEOMETRY = 1ull << 0ull,
	UNIFORM_FRAGMENT = 1ull << 1ull,
	UNIFORM_COMPUTE = 1ull << 2ull,
	UNIFORM_TRACE_RAYS = 1ull << 3ull,

	STORAGE_GEOMETRY_READ = 1ull << 4ull,
	STORAGE_GEOMETRY_WRITE = 1ull << 5ull,
	STORAGE_FRAGMENT_READ = 1ull << 6ull,
	STORAGE_FRAGMENT_WRITE = 1ull << 7ull,
	STORAGE_COMPUTE_READ = 1ull << 8ull,
	STORAGE_COMPUTE_WRITE = 1ull << 9ull,
	STORAGE_TRACE_RAYS_READ = 1ull << 10ull,
	STORAGE_TRACE_RAYS_WRITE = 1ull << 11ull,

	TEXTURE_GEOMETRY_READ = 1ull << 12ull,
	TEXTURE_GEOMETRY_WRITE = 1ull << 13ull,
	TEXTURE_FRAGMENT_READ = 1ull << 14ull,
	TEXTURE_FRAGMENT_WRITE = 1ull << 15ull,
	TEXTURE_COMPUTE_READ = 1ull << 16ull,
	TEXTURE_COMPUTE_WRITE = 1ull << 17ull,
	TEXTURE_TRACE_RAYS_READ = 1ull << 18ull,
	TEXTURE_TRACE_RAYS_WRITE = 1ull << 19ull,

	INDEX = 1ull << 20ull,
	VERTEX = 1ull << 21ull,

	INDIRECT_COMPUTE = 1ull << 22ll,
	INDIRECT_DRAW = 1ull << 23ull,
	INDIRECT_TRACE_RAYS = 1ull << 24ull,

	TRANSFER_SOURCE = 1ull << 25ull,
	TRANSFER_DESTINATION = 1ull << 26ull,

	ACCELERATION_STRUCTURE_BUILD = 1ull << 27ull, ///< Will be used as a position or index buffer in a BLAS build.
	SBT = 1ull << 28ull, ///< Will be used as SBT in a traceRays() command.

	// Derived
	ALL_UNIFORM = UNIFORM_GEOMETRY | UNIFORM_FRAGMENT | UNIFORM_COMPUTE | UNIFORM_TRACE_RAYS,
	ALL_STORAGE = STORAGE_GEOMETRY_READ | STORAGE_GEOMETRY_WRITE | STORAGE_FRAGMENT_READ | STORAGE_FRAGMENT_WRITE
				  | STORAGE_COMPUTE_READ | STORAGE_COMPUTE_WRITE | STORAGE_TRACE_RAYS_READ | STORAGE_TRACE_RAYS_WRITE,
	ALL_TEXTURE = TEXTURE_GEOMETRY_READ | TEXTURE_GEOMETRY_WRITE | TEXTURE_FRAGMENT_READ | TEXTURE_FRAGMENT_WRITE
				  | TEXTURE_COMPUTE_READ | TEXTURE_COMPUTE_WRITE | TEXTURE_TRACE_RAYS_READ | TEXTURE_TRACE_RAYS_WRITE,
	ALL_INDIRECT = INDIRECT_COMPUTE | INDIRECT_DRAW | INDIRECT_TRACE_RAYS,
	ALL_TRANSFER = TRANSFER_SOURCE | TRANSFER_DESTINATION,

	ALL_GEOMETRY = UNIFORM_GEOMETRY | STORAGE_GEOMETRY_READ | STORAGE_GEOMETRY_WRITE | TEXTURE_GEOMETRY_READ
				   | TEXTURE_GEOMETRY_WRITE | INDEX | VERTEX,
	ALL_FRAGMENT = UNIFORM_FRAGMENT | STORAGE_FRAGMENT_READ | STORAGE_FRAGMENT_WRITE | TEXTURE_FRAGMENT_READ
				   | TEXTURE_FRAGMENT_WRITE,
	ALL_GRAPHICS = ALL_GEOMETRY | ALL_FRAGMENT | INDIRECT_DRAW,
	ALL_COMPUTE = UNIFORM_COMPUTE | STORAGE_COMPUTE_READ | STORAGE_COMPUTE_WRITE | TEXTURE_COMPUTE_READ
				  | TEXTURE_COMPUTE_WRITE | INDIRECT_COMPUTE,
	ALL_TRACE_RAYS = UNIFORM_TRACE_RAYS | STORAGE_TRACE_RAYS_READ | STORAGE_TRACE_RAYS_WRITE | TEXTURE_TRACE_RAYS_READ
					 | TEXTURE_TRACE_RAYS_WRITE | INDIRECT_TRACE_RAYS | SBT,

	ALL_RAY_TRACING = ALL_TRACE_RAYS | ACCELERATION_STRUCTURE_BUILD,
	ALL_READ = ALL_UNIFORM | STORAGE_GEOMETRY_READ | STORAGE_FRAGMENT_READ | STORAGE_COMPUTE_READ
			   | STORAGE_TRACE_RAYS_READ | TEXTURE_GEOMETRY_READ | TEXTURE_FRAGMENT_READ | TEXTURE_COMPUTE_READ
			   | TEXTURE_TRACE_RAYS_READ | INDEX | VERTEX | INDIRECT_COMPUTE | INDIRECT_DRAW | INDIRECT_TRACE_RAYS
			   | TRANSFER_SOURCE | ACCELERATION_STRUCTURE_BUILD | SBT,
	ALL_WRITE = STORAGE_GEOMETRY_WRITE | STORAGE_FRAGMENT_WRITE | STORAGE_COMPUTE_WRITE | STORAGE_TRACE_RAYS_WRITE
				| TEXTURE_GEOMETRY_WRITE | TEXTURE_FRAGMENT_WRITE | TEXTURE_COMPUTE_WRITE | TEXTURE_TRACE_RAYS_WRITE
				| TRANSFER_DESTINATION,
	ALL = ALL_READ | ALL_WRITE,
};
ANKI_ENUM_ALLOW_NUMERIC_OPERATIONS(BufferUsageBit)

/// Buffer access when mapped.
enum class BufferMapAccessBit : U8
{
	NONE = 0,
	READ = 1 << 0,
	WRITE = 1 << 1
};
ANKI_ENUM_ALLOW_NUMERIC_OPERATIONS(BufferMapAccessBit)

/// Index buffer's index type.
enum class IndexType : U8
{
	U16,
	U32,
	COUNT
};

/// Rasterization order.
enum class RasterizationOrder : U8
{
	ORDERED,
	RELAXED,
	COUNT
};

/// Acceleration structure type.
enum class AccelerationStructureType : U8
{
	TOP_LEVEL,
	BOTTOM_LEVEL,
	COUNT
};

enum class AccelerationStructureUsageBit : U8
{
	NONE = 0,
	BUILD = 1 << 0,
	ATTACH = 1 << 1, ///< Attached to a TLAS. Only for BLAS.
	GEOMETRY_READ = 1 << 2,
	FRAGMENT_READ = 1 << 3,
	COMPUTE_READ = 1 << 4,
	TRACE_RAYS_READ = 1 << 5,

	// Derived
	ALL_GRAPHICS = GEOMETRY_READ | FRAGMENT_READ,
	ALL_READ = ATTACH | GEOMETRY_READ | FRAGMENT_READ | COMPUTE_READ | TRACE_RAYS_READ,
	ALL_WRITE = BUILD
};
ANKI_ENUM_ALLOW_NUMERIC_OPERATIONS(AccelerationStructureUsageBit)

/// VRS rates.
enum class VrsRate : U8
{
	_1x1, ///< Disable VRS. Always supported.
	_2x1, ///< Always supported.
	_1x2,
	_2x2, ///< Always supported.
	_4x2,
	_2x4,
	_4x4,

	COUNT,
	FIRST = 0
};
ANKI_ENUM_ALLOW_NUMERIC_OPERATIONS(VrsRate)

/// The draw indirect structure for index drawing, also the parameters of a regular drawcall
class DrawElementsIndirectInfo
{
public:
	U32 m_count = MAX_U32;
	U32 m_instanceCount = 1;
	U32 m_firstIndex = 0;
	U32 m_baseVertex = 0;
	U32 m_baseInstance = 0;

	DrawElementsIndirectInfo() = default;

	DrawElementsIndirectInfo(const DrawElementsIndirectInfo&) = default;

	DrawElementsIndirectInfo(U32 count, U32 instanceCount, U32 firstIndex, U32 baseVertex, U32 baseInstance)
		: m_count(count)
		, m_instanceCount(instanceCount)
		, m_firstIndex(firstIndex)
		, m_baseVertex(baseVertex)
		, m_baseInstance(baseInstance)
	{
	}

	Bool operator==(const DrawElementsIndirectInfo& b) const
	{
		return m_count == b.m_count && m_instanceCount == b.m_instanceCount && m_firstIndex == b.m_firstIndex
			   && m_baseVertex == b.m_baseVertex && m_baseInstance == b.m_baseInstance;
	}

	Bool operator!=(const DrawElementsIndirectInfo& b) const
	{
		return !(operator==(b));
	}
};

/// The draw indirect structure for arrays drawing, also the parameters of a regular drawcall
class DrawArraysIndirectInfo
{
public:
	U32 m_count = MAX_U32;
	U32 m_instanceCount = 1;
	U32 m_first = 0;
	U32 m_baseInstance = 0;

	DrawArraysIndirectInfo() = default;

	DrawArraysIndirectInfo(const DrawArraysIndirectInfo&) = default;

	DrawArraysIndirectInfo(U32 count, U32 instanceCount, U32 first, U32 baseInstance)
		: m_count(count)
		, m_instanceCount(instanceCount)
		, m_first(first)
		, m_baseInstance(baseInstance)
	{
	}

	Bool operator==(const DrawArraysIndirectInfo& b) const
	{
		return m_count == b.m_count && m_instanceCount == b.m_instanceCount && m_first == b.m_first
			   && m_baseInstance == b.m_baseInstance;
	}

	Bool operator!=(const DrawArraysIndirectInfo& b) const
	{
		return !(operator==(b));
	}
};

/// Clear values for textures or attachments.
class ClearValue
{
private:
	class Ds
	{
	public:
		F32 m_depth;
		I32 m_stencil;
	};

public:
	union
	{
		Array<F32, 4> m_colorf;
		Array<I32, 4> m_colori;
		Array<U32, 4> m_coloru;
		Ds m_depthStencil;
	};

	ClearValue()
	{
		zeroMemory(*this);
	}

	ClearValue(const ClearValue& b)
	{
		operator=(b);
	}

	ClearValue& operator=(const ClearValue& b)
	{
		memcpy(this, &b, sizeof(*this));
		return *this;
	}
};

/// A way to identify a surface in a texture.
class TextureSurfaceInfo
{
public:
	U32 m_level = 0;
	U32 m_depth = 0;
	U32 m_face = 0;
	U32 m_layer = 0;

	constexpr TextureSurfaceInfo() = default;

	constexpr TextureSurfaceInfo(const TextureSurfaceInfo&) = default;

	constexpr TextureSurfaceInfo(U32 level, U32 depth, U32 face, U32 layer)
		: m_level(level)
		, m_depth(depth)
		, m_face(face)
		, m_layer(layer)
	{
	}

	TextureSurfaceInfo& operator=(const TextureSurfaceInfo&) = default;

	Bool operator==(const TextureSurfaceInfo& b) const
	{
		return m_level == b.m_level && m_depth == b.m_depth && m_face == b.m_face && m_layer == b.m_layer;
	}

	Bool operator!=(const TextureSurfaceInfo& b) const
	{
		return !(*this == b);
	}

	U64 computeHash() const
	{
		return anki::computeHash(this, sizeof(*this), 0x1234567);
	}

	static TextureSurfaceInfo newZero()
	{
		return TextureSurfaceInfo();
	}
};

/// A way to identify a volume in 3D textures.
class TextureVolumeInfo
{
public:
	U32 m_level = 0;

	TextureVolumeInfo() = default;

	TextureVolumeInfo(const TextureVolumeInfo&) = default;

	TextureVolumeInfo(U32 level)
		: m_level(level)
	{
	}

	TextureVolumeInfo& operator=(const TextureVolumeInfo&) = default;
};

/// Defines a subset of a texture.
class TextureSubresourceInfo
{
public:
	U32 m_firstMipmap = 0;
	U32 m_mipmapCount = 1;

	U32 m_firstLayer = 0;
	U32 m_layerCount = 1;

	U8 m_firstFace = 0;
	U8 m_faceCount = 1;

	DepthStencilAspectBit m_depthStencilAspect = DepthStencilAspectBit::kNone;

	U8 _m_padding[1] = {0};

	constexpr TextureSubresourceInfo(DepthStencilAspectBit aspect = DepthStencilAspectBit::kNone)
		: m_depthStencilAspect(aspect)
	{
	}

	TextureSubresourceInfo(const TextureSubresourceInfo&) = default;

	constexpr TextureSubresourceInfo(const TextureSurfaceInfo& surf,
									 DepthStencilAspectBit aspect = DepthStencilAspectBit::kNone)
		: m_firstMipmap(surf.m_level)
		, m_mipmapCount(1)
		, m_firstLayer(surf.m_layer)
		, m_layerCount(1)
		, m_firstFace(U8(surf.m_face))
		, m_faceCount(1)
		, m_depthStencilAspect(aspect)
	{
	}

	constexpr TextureSubresourceInfo(const TextureVolumeInfo& vol,
									 DepthStencilAspectBit aspect = DepthStencilAspectBit::kNone)
		: m_firstMipmap(vol.m_level)
		, m_mipmapCount(1)
		, m_firstLayer(0)
		, m_layerCount(1)
		, m_firstFace(0)
		, m_faceCount(1)
		, m_depthStencilAspect(aspect)
	{
	}

	TextureSubresourceInfo& operator=(const TextureSubresourceInfo&) = default;

	Bool operator==(const TextureSubresourceInfo& b) const
	{
		ANKI_ASSERT(_m_padding[0] == b._m_padding[0]);
		return memcmp(this, &b, sizeof(*this)) == 0;
	}

	Bool operator!=(const TextureSubresourceInfo& b) const
	{
		return !(*this == b);
	}

	U64 computeHash() const
	{
		static_assert(sizeof(*this) == sizeof(U32) * 4 + sizeof(U8) * 4, "Should be hashable");
		ANKI_ASSERT(_m_padding[0] == 0);
		return anki::computeHash(this, sizeof(*this));
	}

	Bool overlapsWith(const TextureSubresourceInfo& b) const
	{
		auto overlaps = [](U32 beginA, U32 countA, U32 beginB, U32 countB) -> Bool {
			return (beginA < beginB) ? (beginA + countA > beginB) : (beginB + countB > beginA);
		};

		const Bool depthStencilOverlaps = (m_depthStencilAspect == DepthStencilAspectBit::kNone
										   && b.m_depthStencilAspect == DepthStencilAspectBit::kNone)
										  || !!(m_depthStencilAspect & b.m_depthStencilAspect);

		return overlaps(m_firstMipmap, m_mipmapCount, b.m_firstMipmap, b.m_mipmapCount)
			   && overlaps(m_firstLayer, m_layerCount, b.m_firstLayer, b.m_layerCount)
			   && overlaps(m_firstFace, m_faceCount, b.m_firstFace, b.m_faceCount) && depthStencilOverlaps;
	}
};

class TextureBarrierInfo
{
public:
	Texture* m_texture = nullptr;
	TextureUsageBit m_previousUsage = TextureUsageBit::kNone;
	TextureUsageBit m_nextUsage = TextureUsageBit::kNone;
	TextureSubresourceInfo m_subresource;
};

class BufferBarrierInfo
{
public:
	Buffer* m_buffer = nullptr;
	BufferUsageBit m_previousUsage = BufferUsageBit::NONE;
	BufferUsageBit m_nextUsage = BufferUsageBit::NONE;
	PtrSize m_offset = 0;
	PtrSize m_size = 0;
};

class AccelerationStructureBarrierInfo
{
public:
	AccelerationStructure* m_as = nullptr;
	AccelerationStructureUsageBit m_previousUsage = AccelerationStructureUsageBit::NONE;
	AccelerationStructureUsageBit m_nextUsage = AccelerationStructureUsageBit::NONE;
};

/// Compute max number of mipmaps for a 2D texture.
U32 computeMaxMipmapCount2d(U32 w, U32 h, U32 minSizeOfLastMip = 1);

/// Compute max number of mipmaps for a 3D texture.
U32 computeMaxMipmapCount3d(U32 w, U32 h, U32 d, U32 minSizeOfLastMip = 1);
/// @}

} // end namespace anki
