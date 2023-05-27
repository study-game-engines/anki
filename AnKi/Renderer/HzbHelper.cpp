// Copyright (C) 2009-2023, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <AnKi/Renderer/HzbHelper.h>
#include <AnKi/Renderer/Renderer.h>

#if ANKI_COMPILER_GCC_COMPATIBLE
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wunused-function"
#	pragma GCC diagnostic ignored "-Wignored-qualifiers"
#elif ANKI_COMPILER_MSVC
#	pragma warning(push)
#	pragma warning(disable : 4505)
#endif
#define A_CPU
#include <ThirdParty/FidelityFX/ffx_a.h>
#include <ThirdParty/FidelityFX/ffx_spd.h>
#if ANKI_COMPILER_GCC_COMPATIBLE
#	pragma GCC diagnostic pop
#elif ANKI_COMPILER_MSVC
#	pragma warning(pop)
#endif

namespace anki {

//   7 +----+ 6
//    /|   /|
// 3 +----+2|
//   | *--| + 5
//   |/4  |/
// 0 +----+  1
static constexpr U16 kBoxIndices[] = {1, 5, 2, 2, 5, 6, 0, 3, 4, 4, 3, 7, 3, 2, 7, 7, 2, 6, 0, 4, 1, 1, 4, 5, 0, 1, 3, 3, 1, 2, 4, 7, 5, 5, 7, 6};

Error HzbHelper::init()
{
	if(GrManager::getSingleton().getDeviceCapabilities().m_samplingFilterMinMax)
	{
		SamplerInitInfo sinit("HzbReductionMax");
		sinit.m_addressing = SamplingAddressing::kClamp;
		sinit.m_mipmapFilter = SamplingFilter::kMax;
		sinit.m_minMagFilter = SamplingFilter::kMax;
		m_maxSampler = GrManager::getSingleton().newSampler(sinit);
	}

	{
		ANKI_CHECK(ResourceManager::getSingleton().loadResource("ShaderBinaries/HzbGenPyramid.ankiprogbin", m_genPyramidProg));

		ShaderProgramResourceVariantInitInfo variantInit(m_genPyramidProg);
		variantInit.addMutation("REDUCTION_TYPE", 1);
		variantInit.addMutation("REVERSE_1_TO_0", 0);
		variantInit.addMutation("MIN_MAX_SAMPLER", m_maxSampler.isCreated());
		const ShaderProgramResourceVariant* variant;
		m_genPyramidProg->getOrCreateVariant(variantInit, variant);
		m_genPyramidMainCameraGrProg.reset(&variant->getProgram());

		variantInit.addMutation("REVERSE_1_TO_0", 1);
		variantInit.addMutation("MIN_MAX_SAMPLER", 0);
		m_genPyramidProg->getOrCreateVariant(variantInit, variant);
		m_genPyramidShadowGrProg.reset(&variant->getProgram());
	}

	ANKI_CHECK(loadShaderProgram("ShaderBinaries/HzbMinMaxDepth.ankiprogbin", m_minMaxDepthProg, m_minMaxDepthGrProg));
	ANKI_CHECK(loadShaderProgram("ShaderBinaries/HzbMinMaxBox.ankiprogbin", m_minMaxBoxProg, m_minMaxBoxGrProg));

	BufferInitInfo buffInit("HzbCounterBuffer");
	buffInit.m_size = sizeof(U32);
	buffInit.m_usage = BufferUsageBit::kStorageComputeWrite | BufferUsageBit::kTransferDestination;
	m_counterBuffer = GrManager::getSingleton().newBuffer(buffInit);

	// Zero counter buffer
	{
		CommandBufferInitInfo cmdbInit;
		cmdbInit.m_flags |= CommandBufferFlag::kSmallBatch;
		CommandBufferPtr cmdb = GrManager::getSingleton().newCommandBuffer(cmdbInit);

		cmdb->fillBuffer(m_counterBuffer.get(), 0, kMaxPtrSize, 0);

		FencePtr fence;
		cmdb->flush({}, &fence);

		fence->clientWait(6.0_sec);
	}

	buffInit = BufferInitInfo("HzbBoxIndices");
	buffInit.m_size = sizeof(kBoxIndices);
	buffInit.m_usage = BufferUsageBit::kIndex;
	buffInit.m_mapAccess = BufferMapAccessBit::kWrite;
	m_boxIndexBuffer = GrManager::getSingleton().newBuffer(buffInit);

	void* mappedMem = m_boxIndexBuffer->map(0, kMaxPtrSize, BufferMapAccessBit::kWrite);
	memcpy(mappedMem, kBoxIndices, sizeof(kBoxIndices));
	m_boxIndexBuffer->unmap();

	m_fbDescr.m_depthStencilAttachment.m_aspect = DepthStencilAspectBit::kDepth;
	m_fbDescr.m_depthStencilAttachment.m_clearValue.m_depthStencil.m_depth = 1.0f;
	m_fbDescr.m_depthStencilAttachment.m_loadOperation = AttachmentLoadOperation::kClear;
	m_fbDescr.bake();

	return Error::kNone;
}

void HzbHelper::populateRenderGraphInternal(RenderTargetHandle srcDepthRt, UVec2 srcDepthRtSize, RenderTargetHandle dstHzbRt, UVec2 dstHzbRtSize,
											RenderGraphDescription& rgraph, CString customName, ShaderProgram* prog, Sampler* sampler) const
{
	TextureSubresourceInfo firstMipSubresource;

	const U32 hzbMipCount = min(kMaxSpdMips, computeMaxMipmapCount2d(dstHzbRtSize.x(), dstHzbRtSize.y()));

	ComputeRenderPassDescription& pass = rgraph.newComputeRenderPass((customName.isEmpty()) ? "HZB generation" : customName);

	pass.newTextureDependency(srcDepthRt, TextureUsageBit::kSampledCompute, firstMipSubresource);
	pass.newTextureDependency(dstHzbRt, TextureUsageBit::kImageComputeWrite);

	pass.setWork([this, hzbMipCount, srcDepthRt, srcDepthRtSize, dstHzbRt, dstHzbRtSize, prog, sampler](RenderPassWorkContext& rgraphCtx) {
		CommandBuffer& cmdb = *rgraphCtx.m_commandBuffer;

		const U32 mipsToCompute = hzbMipCount;

		cmdb.bindShaderProgram(prog);

		varAU2(dispatchThreadGroupCountXY);
		varAU2(workGroupOffset); // needed if Left and Top are not 0,0
		varAU2(numWorkGroupsAndMips);
		varAU4(rectInfo) = initAU4(0, 0, dstHzbRtSize.x() * 2, dstHzbRtSize.y() * 2);
		SpdSetup(dispatchThreadGroupCountXY, workGroupOffset, numWorkGroupsAndMips, rectInfo, mipsToCompute);

		struct Uniforms
		{
			Vec2 m_invSrcTexSize;
			U32 m_threadGroupCount;
			U32 m_mipmapCount;
		} pc;

		pc.m_invSrcTexSize = 1.0f / Vec2(dstHzbRtSize * 2);
		pc.m_threadGroupCount = numWorkGroupsAndMips[0];
		pc.m_mipmapCount = numWorkGroupsAndMips[1];

		cmdb.setPushConstants(&pc, sizeof(pc));

		for(U32 mip = 0; mip < kMaxSpdMips; ++mip)
		{
			TextureSubresourceInfo subresource;
			if(mip < mipsToCompute)
			{
				subresource.m_firstMipmap = mip;
			}
			else
			{
				subresource.m_firstMipmap = 0; // Put something random
			}

			rgraphCtx.bindImage(0, 0, dstHzbRt, subresource, mip);
		}

		cmdb.bindStorageBuffer(0, 1, m_counterBuffer.get(), 0, kMaxPtrSize);
		rgraphCtx.bindTexture(0, 2, srcDepthRt, TextureSubresourceInfo(DepthStencilAspectBit::kDepth));
		cmdb.bindSampler(0, 3, sampler);

		cmdb.dispatchCompute(dispatchThreadGroupCountXY[0], dispatchThreadGroupCountXY[1], 1);
	});
}

void HzbHelper::populateRenderGraph(RenderTargetHandle srcDepthRt, UVec2 srcDepthRtSize, RenderTargetHandle dstHzbRt, UVec2 dstHzbRtSize,
									RenderGraphDescription& rgraph, CString customName) const
{
	populateRenderGraphInternal(srcDepthRt, srcDepthRtSize, dstHzbRt, dstHzbRtSize, rgraph, customName, m_genPyramidMainCameraGrProg.get(),
								m_maxSampler.isCreated() ? m_maxSampler.get() : getRenderer().getSamplers().m_trilinearClamp.get());
}

void HzbHelper::populateRenderGraphDirectionalLight(RenderTargetHandle srcDepthRt, UVec2 srcDepthRtSize, ConstWeakArray<RenderTargetHandle> dstHzbRts,
													ConstWeakArray<Mat4> dstViewProjectionMats, ConstWeakArray<UVec2> dstHzbSizes,
													const Mat4& invViewProjMat, RenderGraphDescription& rgraph) const
{
	RenderTargetHandle minMaxRt;
	constexpr U32 kTileSize = 64;
	const UVec2 minMaxRtSize((srcDepthRtSize.x() + kTileSize - 1) / kTileSize, (srcDepthRtSize.y() + kTileSize - 1) / kTileSize);
	const U32 cascadeCount = dstHzbRts.getSize();
	ANKI_ASSERT(cascadeCount > 0);

	// Generate a temp RT with the min&max depth of each 64x64 tile of the depth buffer
	{
		RenderTargetDescription minMaxRtDescr("HZB min/max depth");
		minMaxRtDescr.m_width = minMaxRtSize.x();
		minMaxRtDescr.m_height = minMaxRtSize.y();
		minMaxRtDescr.m_format = Format::kR32G32_Sfloat;
		minMaxRtDescr.bake();
		minMaxRt = rgraph.newRenderTarget(minMaxRtDescr);

		ComputeRenderPassDescription& pass = rgraph.newComputeRenderPass("HZB min/max depth");

		pass.newTextureDependency(srcDepthRt, TextureUsageBit::kSampledCompute, DepthStencilAspectBit::kDepth);
		pass.newTextureDependency(minMaxRt, TextureUsageBit::kImageComputeWrite);

		pass.setWork([this, srcDepthRt, minMaxRt, minMaxRtSize, srcDepthRtSize](RenderPassWorkContext& rgraphCtx) {
			CommandBuffer& cmdb = *rgraphCtx.m_commandBuffer;

			rgraphCtx.bindTexture(0, 0, srcDepthRt, TextureSubresourceInfo(DepthStencilAspectBit::kDepth));
			cmdb.bindSampler(0, 1, getRenderer().getSamplers().m_nearestNearestClamp.get());
			rgraphCtx.bindImage(0, 2, minMaxRt);

			cmdb.bindShaderProgram(m_minMaxDepthGrProg.get());

			cmdb.dispatchCompute(minMaxRtSize.x(), minMaxRtSize.y(), 1);
		});
	}

	// Project a box for each tile on each cascade's HZB
	Array<RenderTargetHandle, kMaxShadowCascades> depthRts;
	for(U32 i = 0; i < cascadeCount; ++i)
	{
		RenderTargetDescription depthRtDescr("HZB min/max boxes depth");
		depthRtDescr.m_width = dstHzbSizes[i].x() * 2;
		depthRtDescr.m_height = dstHzbSizes[i].y() * 2;
		depthRtDescr.m_format = Format::kD16_Unorm;
		depthRtDescr.bake();
		depthRts[i] = rgraph.newRenderTarget(depthRtDescr);

		GraphicsRenderPassDescription& pass = rgraph.newGraphicsRenderPass("HZB min/max boxes");

		pass.setFramebufferInfo(m_fbDescr, {}, depthRts[i]);

		pass.newTextureDependency(minMaxRt, TextureUsageBit::kSampledFragment);
		pass.newTextureDependency(depthRts[i], TextureUsageBit::kFramebufferWrite, DepthStencilAspectBit::kDepth);

		pass.setWork([this, minMaxRt, invViewProjMat, lightViewProjMat = dstViewProjectionMats[i], viewport = dstHzbSizes[i] * 2, minMaxRtSize,
					  srcDepthRtSize](RenderPassWorkContext& rgraphCtx) {
			CommandBuffer& cmdb = *rgraphCtx.m_commandBuffer;

			cmdb.setViewport(0, 0, viewport.x(), viewport.y());

			cmdb.bindShaderProgram(m_minMaxBoxGrProg.get());

			rgraphCtx.bindColorTexture(0, 0, minMaxRt);

			struct Uniforms
			{
				Mat4 m_reprojectionMat;
				UVec2 m_mainCameraDepthBufferSize;
				UVec2 m_padding;
			} unis;

			unis.m_reprojectionMat = lightViewProjMat * invViewProjMat;
			unis.m_mainCameraDepthBufferSize = srcDepthRtSize;

			cmdb.setPushConstants(&unis, sizeof(unis));

			cmdb.bindIndexBuffer(m_boxIndexBuffer.get(), 0, IndexType::kU16);

			cmdb.drawIndexed(PrimitiveTopology::kTriangles, 6 * 2 * 3, minMaxRtSize.x() * minMaxRtSize.y());
		});
	}

	// Generate the HZBs
	for(U32 i = 0; i < cascadeCount; ++i)
	{
		populateRenderGraphInternal(depthRts[i], dstHzbSizes[i] * 2, dstHzbRts[i], dstHzbSizes[i], rgraph, "HZB generation cascade",
									m_genPyramidShadowGrProg.get(), getRenderer().getSamplers().m_trilinearClamp.get());
	}
}

} // end namespace anki
