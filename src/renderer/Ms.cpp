// Copyright (C) 2014, Panagiotis Christopoulos Charitos.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include "anki/renderer/Ms.h"
#include "anki/renderer/Ez.h"
#include "anki/renderer/Renderer.h"

#include "anki/util/Logger.h"
#include "anki/scene/Camera.h"
#include "anki/scene/SceneGraph.h"
#include "anki/misc/ConfigSet.h"

namespace anki {

//==============================================================================
Ms::~Ms()
{}

//==============================================================================
void Ms::createRt(U32 index, U32 samples)
{
	Plane& plane = m_planes[index];

	m_r->createRenderTarget(m_r->getWidth(), m_r->getHeight(),
		GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT,
		GL_UNSIGNED_INT, samples, plane.m_depthRt);

	m_r->createRenderTarget(m_r->getWidth(), m_r->getHeight(), GL_RGBA8,
			GL_RGBA, GL_UNSIGNED_BYTE, samples, plane.m_rt0);

	m_r->createRenderTarget(m_r->getWidth(), m_r->getHeight(), GL_RGBA8,
		GL_RGBA, GL_UNSIGNED_BYTE, samples, plane.m_rt1);

	GlDevice& gl = getGlDevice();
	GlCommandBufferHandle cmdb;
	cmdb.create(&gl);

	plane.m_fb.create(
		cmdb,
		{{plane.m_rt0, GL_COLOR_ATTACHMENT0}, 
		{plane.m_rt1, GL_COLOR_ATTACHMENT1},
		{plane.m_depthRt, GL_DEPTH_ATTACHMENT}});

	cmdb.finish();
}

//==============================================================================
void Ms::init(const ConfigSet& initializer)
{
	try
	{
		if(initializer.get("samples") > 1)
		{
			createRt(0, initializer.get("samples"));
		}
		createRt(1, 1);

		// Init small depth 
		{
			m_smallDepthSize = UVec2(
				getAlignedRoundDown(16, m_r->getWidth() / 3),
				getAlignedRoundDown(16, m_r->getHeight() / 3));

			m_r->createRenderTarget(
				m_smallDepthSize.x(), 
				m_smallDepthSize.y(),
				GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT,
				GL_UNSIGNED_INT, 1, m_smallDepthRt);

			GlDevice& gl = getGlDevice();
			GlCommandBufferHandle cmdb;
			cmdb.create(&gl);

			m_smallDepthRt.setFilter(cmdb, GlTextureHandle::Filter::LINEAR);

			m_smallDepthFb.create(
				cmdb,
				{{m_smallDepthRt, GL_DEPTH_ATTACHMENT}});

			cmdb.finish();
		}

		m_ez.init(initializer);
	}
	catch(const std::exception& e)
	{
		throw ANKI_EXCEPTION("Failed to initialize material stage") << e;
	}
}

//==============================================================================
void Ms::run(GlCommandBufferHandle& cmdb)
{
	// Chose the multisampled or the singlesampled framebuffer
	if(m_r->getSamples() > 1)
	{
		m_planes[0].m_fb.bind(cmdb, true);
	}
	else
	{
		m_planes[1].m_fb.bind(cmdb, true);
	}

	cmdb.setViewport(0, 0, m_r->getWidth(), m_r->getHeight());
	cmdb.clearBuffers(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT
#if ANKI_DEBUG
		| GL_COLOR_BUFFER_BIT
#endif
			);

	cmdb.enableDepthTest(true);
	cmdb.setDepthWriteMask(true);

	if(m_ez.getEnabled())
	{
		cmdb.setDepthFunction(GL_LESS);
		cmdb.setColorWriteMask(false, false, false, false);

		m_ez.run(cmdb);

		cmdb.setDepthFunction(GL_LEQUAL);
		cmdb.setColorWriteMask(true, true, true, true);
	}

	// render all
	m_r->getSceneDrawer().prepareDraw(RenderingStage::MATERIAL, Pass::COLOR,
		cmdb);

	VisibilityTestResults& vi =
		m_r->getSceneGraph().getActiveCamera().getVisibilityTestResults();

	Camera& cam = m_r->getSceneGraph().getActiveCamera();
	for(auto& it : vi.m_renderables)
	{
		m_r->getSceneDrawer().render(cam, it);
	}

	m_r->getSceneDrawer().finishDraw();

	// If there is multisampling then resolve to singlesampled
	if(m_r->getSamples() > 1)
	{
#if 0
		fbo[1].blitFrom(fbo[0], UVec2(0U), UVec2(r->getWidth(), r->getHeight()),
			UVec2(0U), UVec2(r->getWidth(), r->getHeight()),
			GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT,
			GL_NEAREST);
#endif
		ANKI_ASSERT(0 && "TODO");
	}

	// Blit big depth buffer to small one
	m_smallDepthFb.blit(cmdb, m_planes[1].m_fb, 
		{{0, 0, m_r->getWidth(), m_r->getHeight()}},
		{{0, 0, m_smallDepthSize.x(), m_smallDepthSize.y()}},
		GL_DEPTH_BUFFER_BIT, false);

	cmdb.enableDepthTest(false);
}

} // end namespace anki
