// Copyright (C) 2009-2022, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <AnKi/Renderer/Scale.h>
#include <AnKi/Renderer/Renderer.h>
#include <AnKi/Renderer/TemporalAA.h>
#include <AnKi/Core/ConfigSet.h>

#include <AnKi/Renderer/LightShading.h>
#include <AnKi/Renderer/MotionVectors.h>
#include <AnKi/Renderer/GBuffer.h>
#include <AnKi/Renderer/Tonemapping.h>

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
#include <ThirdParty/FidelityFX/ffx_fsr1.h>
#if ANKI_COMPILER_GCC_COMPATIBLE
#	pragma GCC diagnostic pop
#elif ANKI_COMPILER_MSVC
#	pragma warning(pop)
#endif

namespace anki {

Scale::~Scale()
{
}

Error Scale::init()
{
	ANKI_R_LOGV("Initializing scale");

	const Bool needsScaling = m_r->getPostProcessResolution() != m_r->getInternalResolution();
	const Bool needsSharpening = getConfig().getRSharpen();
	if(!needsScaling && !needsSharpening)
	{
		return Error::NONE;
	}

	const Bool preferCompute = getConfig().getRPreferCompute();
	const U32 dlssQuality = getConfig().getRDlss();
	const U32 fsrQuality = getConfig().getRFsr();

	if(needsScaling)
	{
		if(dlssQuality > 0 && getGrManager().getDeviceCapabilities().m_dlss)
		{
			m_upscalingMethod = UpscalingMethod::GR;
		}
		else if(fsrQuality > 0)
		{
			m_upscalingMethod = UpscalingMethod::FSR;
		}
		else
		{
			m_upscalingMethod = UpscalingMethod::BILINEAR;
		}
	}
	else
	{
		m_upscalingMethod = UpscalingMethod::NONE;
	}

	if(needsSharpening)
	{
		if(m_upscalingMethod == UpscalingMethod::GR)
		{
			m_sharpenMethod = SharpenMethod::GR;
		}
		else
		{
			m_sharpenMethod = SharpenMethod::RCAS;
		}
	}
	else
	{
		m_sharpenMethod = SharpenMethod::NONE;
	}

	// Scale programs
	if(m_upscalingMethod == UpscalingMethod::BILINEAR)
	{
		const CString shaderFname =
			(preferCompute) ? "ShaderBinaries/BlitCompute.ankiprogbin" : "ShaderBinaries/BlitRaster.ankiprogbin";

		ANKI_CHECK(getResourceManager().loadResource(shaderFname, m_scaleProg));

		const ShaderProgramResourceVariant* variant;
		m_scaleProg->getOrCreateVariant(variant);
		m_scaleGrProg = variant->getProgram();
	}
	else if(m_upscalingMethod == UpscalingMethod::FSR)
	{
		const CString shaderFname =
			(preferCompute) ? "ShaderBinaries/FsrCompute.ankiprogbin" : "ShaderBinaries/FsrRaster.ankiprogbin";

		ANKI_CHECK(getResourceManager().loadResource(shaderFname, m_scaleProg));

		ShaderProgramResourceVariantInitInfo variantInitInfo(m_scaleProg);
		variantInitInfo.addMutation("SHARPEN", 0);
		variantInitInfo.addMutation("FSR_QUALITY", fsrQuality - 1);
		const ShaderProgramResourceVariant* variant;
		m_scaleProg->getOrCreateVariant(variantInitInfo, variant);
		m_scaleGrProg = variant->getProgram();
	}
	else if(m_upscalingMethod == UpscalingMethod::GR)
	{
		GrUpscalerInitInfo inf;
		inf.m_sourceTextureResolution = m_r->getInternalResolution();
		inf.m_targetTextureResolution = m_r->getPostProcessResolution();
		inf.m_upscalerType = GrUpscalerType::DLSS_2;
		inf.m_qualityMode = GrUpscalerQualityMode(dlssQuality - 1);

		m_grUpscaler = getGrManager().newGrUpscaler(inf);
	}

	// Sharpen programs
	if(needsSharpening)
	{
		ANKI_CHECK(getResourceManager().loadResource((preferCompute) ? "ShaderBinaries/FsrCompute.ankiprogbin"
																	 : "ShaderBinaries/FsrRaster.ankiprogbin",
													 m_sharpenProg));
		ShaderProgramResourceVariantInitInfo variantInitInfo(m_sharpenProg);
		variantInitInfo.addMutation("SHARPEN", 1);
		variantInitInfo.addMutation("FSR_QUALITY", 0);
		const ShaderProgramResourceVariant* variant;
		m_sharpenProg->getOrCreateVariant(variantInitInfo, variant);
		m_sharpenGrProg = variant->getProgram();
	}

	// Descriptors
	Format format;
	if(m_upscalingMethod == UpscalingMethod::GR)
	{
		format = m_r->getHdrFormat();
	}
	else if(getGrManager().getDeviceCapabilities().m_unalignedBbpTextureFormats)
	{
		format = Format::R8G8B8_UNORM;
	}
	else
	{
		format = Format::R8G8B8A8_UNORM;
	}

	m_rtDesc = m_r->create2DRenderTargetDescription(m_r->getPostProcessResolution().x(),
													m_r->getPostProcessResolution().y(), format, "Scaling");
	m_rtDesc.bake();

	m_fbDescr.m_colorAttachmentCount = 1;
	m_fbDescr.bake();

	return Error::NONE;
}

void Scale::populateRenderGraph(RenderingContext& ctx)
{
	if(m_upscalingMethod == UpscalingMethod::NONE && m_sharpenMethod == SharpenMethod::NONE)
	{
		m_runCtx.m_scaledRt = m_r->getTemporalAA().getRt();
		m_runCtx.m_sharpenedRt = m_r->getTemporalAA().getRt();
		return;
	}

	RenderGraphDescription& rgraph = ctx.m_renderGraphDescr;
	const Bool preferCompute = getConfig().getRPreferCompute();

	// Scaling
	if(m_upscalingMethod == UpscalingMethod::GR)
	{
		m_runCtx.m_scaledRt = rgraph.newRenderTarget(m_rtDesc);

		ComputeRenderPassDescription& pass = ctx.m_renderGraphDescr.newComputeRenderPass("DLSS");
		pass.newDependency(RenderPassDependency(m_r->getLightShading().getRt(), TextureUsageBit::ALL_READ));
		pass.newDependency(
			RenderPassDependency(m_r->getMotionVectors().getMotionVectorsRt(), TextureUsageBit::ALL_READ));
		pass.newDependency(RenderPassDependency(m_r->getTonemapping().getRt(), TextureUsageBit::ALL_READ));
		pass.newDependency(RenderPassDependency(m_r->getGBuffer().getDepthRt(), TextureUsageBit::ALL_READ,
												TextureSubresourceInfo(DepthStencilAspectBit::DEPTH)));
		pass.newDependency(RenderPassDependency(m_runCtx.m_scaledRt, TextureUsageBit::ALL_WRITE));

		pass.setWork([this, &ctx](RenderPassWorkContext& rgraphCtx) {
			runGrUpscaling(ctx, rgraphCtx);
		});
	}
	else if(m_upscalingMethod == UpscalingMethod::FSR || m_upscalingMethod == UpscalingMethod::BILINEAR)
	{
		m_runCtx.m_scaledRt = rgraph.newRenderTarget(m_rtDesc);

		if(preferCompute)
		{
			ComputeRenderPassDescription& pass = ctx.m_renderGraphDescr.newComputeRenderPass("Scale");
			pass.newDependency(RenderPassDependency(m_r->getTemporalAA().getRt(), TextureUsageBit::SAMPLED_COMPUTE));
			pass.newDependency(RenderPassDependency(m_runCtx.m_scaledRt, TextureUsageBit::IMAGE_COMPUTE_WRITE));

			pass.setWork([this](RenderPassWorkContext& rgraphCtx) {
				runFsrOrBilinearScaling(rgraphCtx);
			});
		}
		else
		{
			GraphicsRenderPassDescription& pass = ctx.m_renderGraphDescr.newGraphicsRenderPass("Scale");
			pass.setFramebufferInfo(m_fbDescr, {m_runCtx.m_scaledRt});

			pass.newDependency(RenderPassDependency(m_r->getTemporalAA().getRt(), TextureUsageBit::SAMPLED_FRAGMENT));
			pass.newDependency(
				RenderPassDependency(m_runCtx.m_scaledRt, TextureUsageBit::FRAMEBUFFER_ATTACHMENT_WRITE));

			pass.setWork([this](RenderPassWorkContext& rgraphCtx) {
				runFsrOrBilinearScaling(rgraphCtx);
			});
		}
	}
	else
	{
		ANKI_ASSERT(m_upscalingMethod == UpscalingMethod::NONE);
	}

	// Sharpenning
	if(m_sharpenMethod == SharpenMethod::RCAS)
	{
		m_runCtx.m_sharpenedRt = rgraph.newRenderTarget(m_rtDesc);
		const RenderTargetHandle inRt =
			(m_upscalingMethod == UpscalingMethod::NONE) ? m_r->getTemporalAA().getRt() : m_runCtx.m_scaledRt;

		if(preferCompute)
		{
			ComputeRenderPassDescription& pass = ctx.m_renderGraphDescr.newComputeRenderPass("Sharpen");
			pass.newDependency(RenderPassDependency(inRt, TextureUsageBit::SAMPLED_COMPUTE));
			pass.newDependency(RenderPassDependency(m_runCtx.m_sharpenedRt, TextureUsageBit::IMAGE_COMPUTE_WRITE));

			pass.setWork([this](RenderPassWorkContext& rgraphCtx) {
				runRcasSharpening(rgraphCtx);
			});
		}
		else
		{
			GraphicsRenderPassDescription& pass = ctx.m_renderGraphDescr.newGraphicsRenderPass("Sharpen");
			pass.setFramebufferInfo(m_fbDescr, {m_runCtx.m_sharpenedRt});

			pass.newDependency(RenderPassDependency(inRt, TextureUsageBit::SAMPLED_FRAGMENT));
			pass.newDependency(
				RenderPassDependency(m_runCtx.m_sharpenedRt, TextureUsageBit::FRAMEBUFFER_ATTACHMENT_WRITE));

			pass.setWork([this](RenderPassWorkContext& rgraphCtx) {
				runRcasSharpening(rgraphCtx);
			});
		}
	}
	else if(m_sharpenMethod == SharpenMethod::GR)
	{
		m_runCtx.m_sharpenedRt = m_runCtx.m_scaledRt;
	}
	else
	{
		ANKI_ASSERT(m_sharpenMethod == SharpenMethod::NONE);
	}
}

void Scale::runFsrOrBilinearScaling(RenderPassWorkContext& rgraphCtx)
{
	CommandBufferPtr& cmdb = rgraphCtx.m_commandBuffer;
	const Bool preferCompute = getConfig().getRPreferCompute();

	cmdb->bindShaderProgram(m_scaleGrProg);

	cmdb->bindSampler(0, 0, m_r->getSamplers().m_trilinearClamp);
	rgraphCtx.bindColorTexture(0, 1, m_r->getTemporalAA().getRt());

	if(preferCompute)
	{
		rgraphCtx.bindImage(0, 2, m_runCtx.m_scaledRt);
	}

	if(m_upscalingMethod == UpscalingMethod::FSR)
	{
		class
		{
		public:
			UVec4 m_fsrConsts0;
			UVec4 m_fsrConsts1;
			UVec4 m_fsrConsts2;
			UVec4 m_fsrConsts3;
			UVec2 m_viewportSize;
			UVec2 m_padding;
		} pc;

		const Vec2 inRez(m_r->getInternalResolution());
		const Vec2 outRez(m_r->getPostProcessResolution());
		FsrEasuCon(&pc.m_fsrConsts0[0], &pc.m_fsrConsts1[0], &pc.m_fsrConsts2[0], &pc.m_fsrConsts3[0], inRez.x(),
				   inRez.y(), inRez.x(), inRez.y(), outRez.x(), outRez.y());

		pc.m_viewportSize = m_r->getPostProcessResolution();

		cmdb->setPushConstants(&pc, sizeof(pc));
	}
	else if(preferCompute)
	{
		class
		{
		public:
			Vec2 m_viewportSize;
			UVec2 m_viewportSizeU;
		} pc;
		pc.m_viewportSize = Vec2(m_r->getPostProcessResolution());
		pc.m_viewportSizeU = m_r->getPostProcessResolution();

		cmdb->setPushConstants(&pc, sizeof(pc));
	}

	if(preferCompute)
	{
		dispatchPPCompute(cmdb, 8, 8, m_r->getPostProcessResolution().x(), m_r->getPostProcessResolution().y());
	}
	else
	{
		cmdb->setViewport(0, 0, m_r->getPostProcessResolution().x(), m_r->getPostProcessResolution().y());
		cmdb->drawArrays(PrimitiveTopology::TRIANGLES, 3);
	}
}

void Scale::runRcasSharpening(RenderPassWorkContext& rgraphCtx)
{
	CommandBufferPtr& cmdb = rgraphCtx.m_commandBuffer;
	const Bool preferCompute = getConfig().getRPreferCompute();

	cmdb->bindShaderProgram(m_sharpenGrProg);

	cmdb->bindSampler(0, 0, m_r->getSamplers().m_trilinearClamp);
	rgraphCtx.bindColorTexture(
		0, 1, (m_upscalingMethod == UpscalingMethod::NONE) ? m_r->getTemporalAA().getRt() : m_runCtx.m_scaledRt);

	if(preferCompute)
	{
		rgraphCtx.bindImage(0, 2, m_runCtx.m_sharpenedRt);
	}

	class
	{
	public:
		UVec4 m_fsrConsts0;
		UVec4 m_fsrConsts1;
		UVec4 m_fsrConsts2;
		UVec4 m_fsrConsts3;
		UVec2 m_viewportSize;
		UVec2 m_padding;
	} pc;

	FsrRcasCon(&pc.m_fsrConsts0[0], 0.2f);

	pc.m_viewportSize = m_r->getPostProcessResolution();

	cmdb->setPushConstants(&pc, sizeof(pc));

	if(preferCompute)
	{
		dispatchPPCompute(cmdb, 8, 8, m_r->getPostProcessResolution().x(), m_r->getPostProcessResolution().y());
	}
	else
	{
		cmdb->setViewport(0, 0, m_r->getPostProcessResolution().x(), m_r->getPostProcessResolution().y());
		cmdb->drawArrays(PrimitiveTopology::TRIANGLES, 3);
	}
}

void Scale::runGrUpscaling(RenderingContext& ctx, RenderPassWorkContext& rgraphCtx)
{
	const Vec2 srcRes(m_r->getInternalResolution());
	const Bool reset = m_r->getFrameCount() == 0;
	const Vec2 mvScale = srcRes; // UV space to Pixel space factor
	// In [-texSize / 2, texSize / 2] -> sub-pixel space {-0.5, 0.5}
	const Vec2 jitterOffset = ctx.m_matrices.m_jitter.getTranslationPart().xy() * srcRes * 0.5f;

	CommandBufferPtr& cmdb = rgraphCtx.m_commandBuffer;

	TextureViewPtr srcView = rgraphCtx.createTextureView(m_r->getLightShading().getRt());
	TextureViewPtr motionVectorsView = rgraphCtx.createTextureView(m_r->getMotionVectors().getMotionVectorsRt());
	TextureViewPtr depthView = rgraphCtx.createTextureView(m_r->getGBuffer().getDepthRt());
	TextureViewPtr exposureView = rgraphCtx.createTextureView(m_r->getTonemapping().getRt());
	TextureViewPtr dstView = rgraphCtx.createTextureView(m_runCtx.m_scaledRt);

	cmdb->upscale(m_grUpscaler, srcView, dstView, motionVectorsView, depthView, exposureView, reset, jitterOffset,
				  mvScale);
}

} // end namespace anki
