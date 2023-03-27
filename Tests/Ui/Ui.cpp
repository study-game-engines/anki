// Copyright (C) 2009-2023, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <Tests/Framework/Framework.h>
#include <AnKi/Core/ConfigSet.h>
#include <AnKi/Util/HighRezTimer.h>
#include <AnKi/Ui.h>
#include <AnKi/Window.h>
#include <AnKi/Core/GpuMemoryPools.h>

using namespace anki;

namespace {

class Label : public UiImmediateModeBuilder
{
public:
	using UiImmediateModeBuilder::UiImmediateModeBuilder;

	Bool m_windowInitialized = false;
	U32 m_buttonClickCount = 0;

	void build(CanvasPtr canvas) final
	{
		Vec4 oldBackground = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
		ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w = 0.8f;

		ImGui::Begin("ImGui Demo", nullptr);

		if(!m_windowInitialized)
		{
			ImGui::SetWindowPos(Vec2(20, 10));
			ImGui::SetWindowSize(Vec2(200, 500));
			m_windowInitialized = true;
		}

		ImGui::Text("Label default size");

		canvas->pushFont(canvas->getDefaultFont(), 30);
		ImGui::Text("Label size 30");
		ImGui::PopFont();

		m_buttonClickCount += ImGui::Button("Toggle");
		if(m_buttonClickCount & 1)
		{
			ImGui::Button("Toggled");
		}

		ImGui::End();
		ImGui::GetStyle().Colors[ImGuiCol_WindowBg] = oldBackground;
	}
};

} // namespace

ANKI_TEST(Ui, Ui)
{
	ConfigSet& cfg = ConfigSet::allocateSingleton(allocAligned, nullptr);
	initConfig(cfg);
	cfg.setGrVsync(true);
	cfg.setGrValidation(false);
	cfg.setWidth(1024);
	cfg.setHeight(760);
	cfg.setRsrcDataPaths("EngineAssets");

	NativeWindow* win = createWindow(cfg);
	ANKI_TEST_EXPECT_NO_ERR(Input::allocateSingleton().init());
	GrManager* gr = createGrManager(win);
	createResourceManager(gr);
	UiManager* ui = new UiManager();

	RebarStagingGpuMemoryPool::allocateSingleton().init(gr);

	HeapAllocator<U8> alloc(allocAligned, nullptr);
	UiManagerInitInfo uiInitInfo;
	uiInitInfo.m_allocCallback = allocAligned;
	uiInitInfo.m_allocCallbackUserData = nullptr;
	uiInitInfo.m_grManager = gr;
	ANKI_TEST_EXPECT_NO_ERR(ui->init(uiInitInfo));

	{
		FontPtr font;
		ANKI_TEST_EXPECT_NO_ERR(ui->newInstance(font, "UbuntuRegular.ttf", Array<U32, 4>{10, 20, 30, 60}));

		CanvasPtr canvas;
		ANKI_TEST_EXPECT_NO_ERR(ui->newInstance(canvas, font, 20, win->getWidth(), win->getHeight()));

		IntrusivePtr<Label> label;
		ANKI_TEST_EXPECT_NO_ERR(ui->newInstance(label));

		Bool done = false;
		while(!done)
		{
			ANKI_TEST_EXPECT_NO_ERR(Input::getSingleton().handleEvents());
			HighRezTimer timer;
			timer.start();

			canvas->handleInput();
			if(Input::getSingleton().getKey(KeyCode::kEscape))
			{
				done = true;
			}

			canvas->beginBuilding();
			label->build(canvas);

			TexturePtr presentTex = gr->acquireNextPresentableTexture();
			FramebufferPtr fb;
			{
				TextureViewInitInfo init;
				init.m_texture = presentTex;
				TextureViewPtr view = gr->newTextureView(init);

				FramebufferInitInfo fbinit;
				fbinit.m_colorAttachmentCount = 1;
				fbinit.m_colorAttachments[0].m_clearValue.m_colorf = {{1.0, 0.0, 1.0, 1.0}};
				fbinit.m_colorAttachments[0].m_textureView = view;

				fb = gr->newFramebuffer(fbinit);
			}

			CommandBufferInitInfo cinit;
			cinit.m_flags = CommandBufferFlag::kGeneralWork | CommandBufferFlag::kSmallBatch;
			CommandBufferPtr cmdb = gr->newCommandBuffer(cinit);

			TextureBarrierInfo barrier;
			barrier.m_previousUsage = TextureUsageBit::kNone;
			barrier.m_nextUsage = TextureUsageBit::kFramebufferWrite;
			barrier.m_texture = presentTex.get();
			cmdb->setPipelineBarrier({&barrier, 1}, {}, {});

			cmdb->beginRenderPass(fb, {{TextureUsageBit::kFramebufferWrite}}, {});
			canvas->appendToCommandBuffer(cmdb);
			cmdb->endRenderPass();

			barrier.m_previousUsage = TextureUsageBit::kFramebufferWrite;
			barrier.m_nextUsage = TextureUsageBit::kPresent;
			cmdb->setPipelineBarrier({&barrier, 1}, {}, {});

			cmdb->flush();

			gr->swapBuffers();
			RebarStagingGpuMemoryPool::getSingleton().endFrame();

			timer.stop();
			const F32 TICK = 1.0f / 30.0f;
			if(timer.getElapsedTime() < TICK)
			{
				HighRezTimer::sleep(TICK - timer.getElapsedTime());
			}
		}
	}

	delete ui;
	RebarStagingGpuMemoryPool::freeSingleton();
	ResourceManager::freeSingleton();
	GrManager::deleteInstance(gr);
	Input::freeSingleton();
	NativeWindow::freeSingleton();
}
