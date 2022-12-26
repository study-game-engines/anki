// Copyright (C) 2009-2022, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <AnKi/Renderer/Drawer.h>
#include <AnKi/Renderer/RenderQueue.h>
#include <AnKi/Resource/ImageResource.h>
#include <AnKi/Renderer/Renderer.h>
#include <AnKi/Util/Tracer.h>
#include <AnKi/Util/Logger.h>
#include <AnKi/Shaders/Include/MaterialTypes.h>
#include <AnKi/Shaders/Include/GpuSceneFunctions.h>

namespace anki {

/// Drawer's context
class RenderableDrawer::Context
{
public:
	RenderQueueDrawContext m_queueCtx;

	const RenderableQueueElement* m_renderableElement = nullptr;

	Array<RenderableQueueElement, kMaxInstanceCount> m_cachedRenderElements;
	Array<U8, kMaxInstanceCount> m_cachedRenderElementLods;
	Array<const void*, kMaxInstanceCount> m_userData;
	U32 m_cachedRenderElementCount = 0;
	U8 m_minLod = 0;
	U8 m_maxLod = 0;
};

/// Check if the drawcalls can be merged.
static Bool canMergeRenderableQueueElements(const RenderableQueueElement& a, const RenderableQueueElement& b)
{
	return a.m_callback == b.m_callback && a.m_mergeKey != 0 && a.m_mergeKey == b.m_mergeKey;
}

RenderableDrawer::~RenderableDrawer()
{
}

void RenderableDrawer::drawRange(RenderingTechnique technique, const RenderableDrawerArguments& args,
								 const RenderableQueueElement* begin, const RenderableQueueElement* end,
								 CommandBufferPtr& cmdb)
{
	ANKI_ASSERT(begin && end && begin < end);

	// Allocate, set and bind global uniforms
	{
		RebarGpuMemoryToken globalUniformsToken;
		MaterialGlobalUniforms* globalUniforms =
			static_cast<MaterialGlobalUniforms*>(m_r->getExternalSubsystems().m_rebarStagingPool->allocateFrame(
				sizeof(MaterialGlobalUniforms), globalUniformsToken));

		globalUniforms->m_viewProjectionMatrix = args.m_viewProjectionMatrix;
		globalUniforms->m_previousViewProjectionMatrix = args.m_previousViewProjectionMatrix;
		static_assert(sizeof(globalUniforms->m_viewTransform) == sizeof(args.m_viewMatrix));
		memcpy(&globalUniforms->m_viewTransform, &args.m_viewMatrix, sizeof(args.m_viewMatrix));
		static_assert(sizeof(globalUniforms->m_cameraTransform) == sizeof(args.m_cameraTransform));
		memcpy(&globalUniforms->m_cameraTransform, &args.m_cameraTransform, sizeof(args.m_cameraTransform));

		cmdb->bindUniformBuffer(U32(MaterialSet::kGlobal), U32(MaterialBinding::kGlobalUniforms),
								m_r->getExternalSubsystems().m_rebarStagingPool->getBuffer(),
								globalUniformsToken.m_offset, globalUniformsToken.m_range);
	}

	// More globals
	cmdb->bindAllBindless(U32(MaterialSet::kBindless));
	cmdb->bindSampler(U32(MaterialSet::kGlobal), U32(MaterialBinding::kTrilinearRepeatSampler), args.m_sampler);
	cmdb->bindStorageBuffer(U32(MaterialSet::kGlobal), U32(MaterialBinding::kGpuScene), args.m_gpuSceneBuffer, 0,
							kMaxPtrSize);

#define _ANKI_BIND_TEXTURE_BUFFER(format) \
	cmdb->bindReadOnlyTextureBuffer(U32(MaterialSet::kGlobal), U32(MaterialBinding::kUnifiedGeometry_##format), \
									args.m_unifiedGeometryBuffer, 0, kMaxPtrSize, Format::k##format)

	_ANKI_BIND_TEXTURE_BUFFER(R32_Sfloat);
	_ANKI_BIND_TEXTURE_BUFFER(R32G32_Sfloat);
	_ANKI_BIND_TEXTURE_BUFFER(R32G32B32_Sfloat);
	_ANKI_BIND_TEXTURE_BUFFER(R32G32B32A32_Sfloat);
	_ANKI_BIND_TEXTURE_BUFFER(R16G16B16A16_Unorm);
	_ANKI_BIND_TEXTURE_BUFFER(R8G8B8A8_Snorm);
	_ANKI_BIND_TEXTURE_BUFFER(R8G8B8A8_Uint);

#undef _ANKI_BIND_TEXTURE_BUFFER

	// Misc
	cmdb->setVertexAttribute(0, 0, Format::kR32_Uint, 0);

	// Set a few things
	Context ctx;
	ctx.m_queueCtx.m_viewMatrix = args.m_viewMatrix;
	ctx.m_queueCtx.m_viewProjectionMatrix = args.m_viewProjectionMatrix;
	ctx.m_queueCtx.m_projectionMatrix = Mat4::getIdentity(); // TODO
	ctx.m_queueCtx.m_previousViewProjectionMatrix = args.m_previousViewProjectionMatrix;
	ctx.m_queueCtx.m_cameraTransform = args.m_cameraTransform;
	ctx.m_queueCtx.m_rebarStagingPool = m_r->getExternalSubsystems().m_rebarStagingPool;
	ctx.m_queueCtx.m_commandBuffer = cmdb;
	ctx.m_queueCtx.m_key = RenderingKey(technique, 0, 1, false, false);
	ctx.m_queueCtx.m_debugDraw = false;
	ctx.m_queueCtx.m_sampler = args.m_sampler;

	ANKI_ASSERT(args.m_minLod < kMaxLodCount && args.m_maxLod < kMaxLodCount && args.m_minLod <= args.m_maxLod);
	ctx.m_minLod = U8(args.m_minLod);
	ctx.m_maxLod = U8(args.m_maxLod);

	for(; begin != end; ++begin)
	{
		ctx.m_renderableElement = begin;

		drawSingle(ctx);
	}

	// Flush the last drawcall
	flushDrawcall(ctx);
}

void RenderableDrawer::flushDrawcall(Context& ctx)
{
	ctx.m_queueCtx.m_key.setLod(ctx.m_cachedRenderElementLods[0]);
	ctx.m_queueCtx.m_key.setInstanceCount(ctx.m_cachedRenderElementCount);

	// Instance buffer
	RebarGpuMemoryToken token;
	PackedGpuSceneRenderableInstance* instances =
		static_cast<PackedGpuSceneRenderableInstance*>(ctx.m_queueCtx.m_rebarStagingPool->allocateFrame(
			sizeof(PackedGpuSceneRenderableInstance) * ctx.m_cachedRenderElementCount, token));
	for(U32 i = 0; i < ctx.m_cachedRenderElementCount; ++i)
	{
		UnpackedGpuSceneRenderableInstance instance;
		instance.m_lod = ctx.m_cachedRenderElementLods[0];
		instance.m_renderableOffset = ctx.m_cachedRenderElements[i].m_renderableOffset;
		instances[i] = packGpuSceneRenderableInstance(instance);
	}

	ctx.m_queueCtx.m_commandBuffer->bindVertexBuffer(0, ctx.m_queueCtx.m_rebarStagingPool->getBuffer(), token.m_offset,
													 sizeof(PackedGpuSceneRenderableInstance),
													 VertexStepRate::kInstance);

	// Draw
	ctx.m_cachedRenderElements[0].m_callback(
		ctx.m_queueCtx, ConstWeakArray<void*>(const_cast<void**>(&ctx.m_userData[0]), ctx.m_cachedRenderElementCount));

	// Rendered something, reset the cached transforms
	if(ctx.m_cachedRenderElementCount > 1)
	{
		ANKI_TRACE_INC_COUNTER(R_MERGED_DRAWCALLS, ctx.m_cachedRenderElementCount - 1);
	}
	ctx.m_cachedRenderElementCount = 0;
}

void RenderableDrawer::drawSingle(Context& ctx)
{
	if(ctx.m_cachedRenderElementCount == kMaxInstanceCount)
	{
		flushDrawcall(ctx);
	}

	const RenderableQueueElement& rqel = *ctx.m_renderableElement;

	const U8 overridenLod = clamp(rqel.m_lod, ctx.m_minLod, ctx.m_maxLod);

	const Bool shouldFlush =
		ctx.m_cachedRenderElementCount > 0
		&& (!canMergeRenderableQueueElements(ctx.m_cachedRenderElements[ctx.m_cachedRenderElementCount - 1], rqel)
			|| ctx.m_cachedRenderElementLods[ctx.m_cachedRenderElementCount - 1] != overridenLod);

	if(shouldFlush)
	{
		flushDrawcall(ctx);
	}

	// Cache the new one
	ctx.m_cachedRenderElements[ctx.m_cachedRenderElementCount] = rqel;
	ctx.m_cachedRenderElementLods[ctx.m_cachedRenderElementCount] = overridenLod;
	ctx.m_userData[ctx.m_cachedRenderElementCount] = rqel.m_userData;
	++ctx.m_cachedRenderElementCount;
}

} // end namespace anki
