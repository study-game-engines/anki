// Copyright (C) 2009-2021, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <AnKi/AnKi.h>

using namespace anki;

class TextureViewerUiNode : public SceneNode
{
public:
	TextureResourcePtr m_textureResource;

	TextureViewerUiNode(SceneGraph* scene, CString name)
		: SceneNode(scene, name)
	{
		SpatialComponent* spatialc = newComponent<SpatialComponent>();
		spatialc->setAlwaysVisible(true);

		UiComponent* uic = newComponent<UiComponent>();
		uic->init([](CanvasPtr& canvas, void* ud) { static_cast<TextureViewerUiNode*>(ud)->draw(canvas); }, this);

		ANKI_CHECK_AND_IGNORE(getSceneGraph().getUiManager().newInstance(m_font, "EngineAssets/UbuntuMonoRegular.ttf",
																		 Array<U32, 1>{16}));

		ANKI_CHECK_AND_IGNORE(getSceneGraph().getResourceManager().loadResource(
			"AnKi/Shaders/UiVisualizeImage.ankiprog", m_imageProgram));
		const ShaderProgramResourceVariant* variant;
		m_imageProgram->getOrCreateVariant(variant);
		m_imageGrProgram = variant->getProgram();
	}

	Error frameUpdate(Second prevUpdateTime, Second crntTime)
	{
		if(!m_textureView.isCreated())
		{
			m_textureView = m_textureResource->getGrTextureView();
		}

		return Error::NONE;
	}

private:
	FontPtr m_font;
	ShaderProgramResourcePtr m_imageProgram;
	ShaderProgramPtr m_imageGrProgram;
	TextureViewPtr m_textureView;
	U32 m_crntMip = 0;
	F32 m_zoom = 1.0f;
	Bool m_pointSampling = true;
	Array<Bool, 4> m_colorChannel = {true, true, true, true};

	void draw(CanvasPtr& canvas)
	{
		const Texture& grTex = *m_textureResource->getGrTexture().get();

		ImGui::Begin("Console", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoTitleBar);

		canvas->pushFont(m_font, 16);

		ImGui::SetWindowPos(Vec2(0.0f, 0.0f));
		ImGui::SetWindowSize(Vec2(F32(canvas->getWidth()), F32(canvas->getHeight())));

		ImGui::BeginChild("Tools", ImVec2(ImGui::GetWindowContentRegionWidth() * 0.25f, -1.0f), true, 0);

		// Info
		ImGui::TextWrapped("Size %ux%u Mips %u", grTex.getWidth(), grTex.getHeight(), grTex.getMipmapCount());

		// Zoom
		ImGui::DragFloat("Zoom", &m_zoom, 0.01f, 0.1f, 20.0f, "%.3f");

		// Sampling
		ImGui::Checkbox("Point sampling", &m_pointSampling);

		// Colors
		ImGui::Checkbox("Red", &m_colorChannel[0]);
		ImGui::SameLine();
		ImGui::Checkbox("Green", &m_colorChannel[1]);
		ImGui::SameLine();
		ImGui::Checkbox("Blue", &m_colorChannel[2]);
		ImGui::SameLine();
		ImGui::Checkbox("Alpha", &m_colorChannel[3]);

		// Mips combo
		{
			Array<CString, 11> mipTexts = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10"};
			CString comboLevel = mipTexts[m_crntMip];
			const U32 lastCrntMip = m_crntMip;
			if(ImGui::BeginCombo("Mipmap", comboLevel.cstr(), ImGuiComboFlags_HeightLarge))
			{
				for(U32 mip = 0; mip < grTex.getMipmapCount(); ++mip)
				{
					const Bool isSelected = (m_crntMip == mip);
					if(ImGui::Selectable(mipTexts[mip].cstr(), isSelected))
					{
						m_crntMip = mip;
					}

					if(isSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			if(lastCrntMip != m_crntMip)
			{
				// Re-create the image view
				TextureViewInitInfo viewInitInf(m_textureResource->getGrTexture());
				viewInitInf.m_firstMipmap = m_crntMip;
				viewInitInf.m_mipmapCount = 1;
				m_textureView = getSceneGraph().getGrManager().newTextureView(viewInitInf);
			}
		}

		ImGui::EndChild();
		ImGui::SameLine();

		// Image
		ImGui::BeginChild("Image", ImVec2(-1.0f, -1.0f), true, ImGuiWindowFlags_HorizontalScrollbar);
		{
			class ExtraPushConstants
			{
			public:
				Vec4 m_colorScale;
			} pc;
			pc.m_colorScale.x() = F32(m_colorChannel[0]);
			pc.m_colorScale.y() = F32(m_colorChannel[1]);
			pc.m_colorScale.z() = F32(m_colorChannel[2]);
			pc.m_colorScale.w() = F32(m_colorChannel[3]);

			canvas->setShaderProgram(m_imageGrProgram, &pc, sizeof(pc));

			ImGui::Image(UiImageId(m_textureView, m_pointSampling),
						 ImVec2(F32(grTex.getWidth()) * m_zoom, F32(grTex.getHeight()) * m_zoom), ImVec2(0.0f, 0.0f),
						 ImVec2(1.0f, 1.0f), Vec4(1.0f), Vec4(0.0f, 0.0f, 0.0f, 1.0f));

			canvas->clearShaderProgram();
		}
		ImGui::EndChild();

		canvas->popFont();
		ImGui::End();
	}
};

class MyApp : public App
{
public:
	Error init(int argc, char** argv, CString appName)
	{
		HeapAllocator<U32> alloc(allocAligned, nullptr);
		StringAuto mainDataPath(alloc, ANKI_SOURCE_DIRECTORY);

		ConfigSet config = DefaultConfigSet::get();
		config.set("window_fullscreen", false);
		config.set("rsrc_dataPaths", mainDataPath);
		config.set("gr_validation", 0);
		ANKI_CHECK(config.setFromCommandLineArguments(argc - 1, argv + 1));

		ANKI_CHECK(App::init(config, allocAligned, nullptr));

		// Load the texture
		if(argc < 2)
		{
			ANKI_LOGE("Wrong number of arguments");
			return Error::USER_DATA;
		}
		TextureResourcePtr tex;
		ANKI_CHECK(getResourceManager().loadResource(argv[1], tex, false));

		// Create the node
		SceneGraph& scene = getSceneGraph();
		TextureViewerUiNode* node;
		ANKI_CHECK(scene.newSceneNode("TextureViewer", node));
		node->m_textureResource = tex;

		return Error::NONE;
	}

	Error userMainLoop(Bool& quit, Second elapsedTime) override
	{
		Input& input = getInput();
		if(input.getKey(KeyCode::ESCAPE))
		{
			quit = true;
		}

		return Error::NONE;
	}
};

int main(int argc, char* argv[])
{
	Error err = Error::NONE;

	MyApp* app = new MyApp;
	err = app->init(argc, argv, "Texture Viewer");
	if(!err)
	{
		err = app->mainLoop();
	}

	delete app;
	if(err)
	{
		ANKI_LOGE("Error reported. Bye!!");
	}
	else
	{
		ANKI_LOGI("Bye!!");
	}

	return 0;
}
