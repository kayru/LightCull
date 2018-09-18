#include "ImGuiImpl.h"

#include <Rush/GfxDevice.h>
#include <Rush/GfxPrimitiveBatch.h>
#include <Rush/Window.h>

#include <imgui.h>

namespace
{

GfxContext*          s_context = nullptr;
Window*              s_window  = nullptr;
GfxBlendState        s_blendState;
GfxDepthStencilState s_depthState;
GfxRasterizerState   s_rasterState;
PrimitiveBatch*      s_prim = nullptr;
GfxTexture           s_fontTexture;

class GuiWMI : public WindowMessageInterceptor
{
public:
	virtual bool processEvent(const WindowEvent& e) override
	{
		ImGuiIO& io = ImGui::GetIO();

		switch (e.type)
		{
		case WindowEventType_Char: io.AddInputCharacter(e.character); return ImGui::GetIO().WantCaptureKeyboard;
		case WindowEventType_KeyDown:
			io.KeysDown[e.code] = true;
			if (e.code == Key_LeftControl || e.code == Key_RightControl)
			{
				io.KeyCtrl = true;
			}
			else if (e.code == Key_LeftShift || e.code == Key_RightShift)
			{
				io.KeyShift = true;
			}
			return ImGui::GetIO().WantCaptureKeyboard;

		case WindowEventType_KeyUp:
			io.KeysDown[e.code] = false;
			if (e.code == Key_LeftControl || e.code == Key_RightControl)
			{
				io.KeyCtrl = false;
			}
			else if (e.code == Key_LeftShift || e.code == Key_RightShift)
			{
				io.KeyShift = false;
			}
			return ImGui::GetIO().WantCaptureKeyboard;

		case WindowEventType_MouseDown:
			if (e.button < 5)
			{
				io.MouseDown[e.button] = true;
			}
			return ImGui::GetIO().WantCaptureMouse;

		case WindowEventType_MouseUp:
			if (e.button < 5)
			{
				io.MouseDown[e.button] = false;
			}
			return ImGui::GetIO().WantCaptureMouse;

		case WindowEventType_MouseMove:
			io.MousePos.x = e.pos.x;
			io.MousePos.y = e.pos.y;
			return ImGui::GetIO().WantCaptureMouse;

		case WindowEventType_Scroll: io.MouseWheel = e.scroll.y; return ImGui::GetIO().WantCaptureMouse;
		default: return false;
		}
	}
};

GuiWMI s_messageInterceptor;
}

static void ImGuiImpl_Render(ImDrawData* drawData)
{
	Gfx_SetBlendState(s_context, s_blendState);
	Gfx_SetRasterizerState(s_context, s_rasterState);
	Gfx_SetDepthStencilState(s_context, s_depthState);

	s_prim->begin2D(s_window->getSize());
	s_prim->setTexture(s_fontTexture);
	s_prim->setSampler(PrimitiveBatch::SamplerState_Point);

	for (int cmdListIndex = 0; cmdListIndex < drawData->CmdListsCount; ++cmdListIndex)
	{
		ImDrawList* cmdList = drawData->CmdLists[cmdListIndex];

		const auto& ib = cmdList->IdxBuffer;
		const auto& vb = cmdList->VtxBuffer;

		u32 indexOffset = 0;
		for (const auto& cmd : cmdList->CmdBuffer)
		{
			auto verts =
			    s_prim->drawVertices(GfxPrimitive::TriangleList, cmd.ElemCount); // TODO: implement indexed drawing
			for (u32 i = 0; i < cmd.ElemCount; ++i)
			{
				u32         index = ib[i + indexOffset];
				const auto& v     = vb[index];

				verts[i].pos.x = v.pos.x;
				verts[i].pos.y = v.pos.y;
				verts[i].pos.z = 1;

				verts[i].tex.x = v.uv.x;
				verts[i].tex.y = v.uv.y;

				verts[i].col   = ColorRGBA8(v.col);
			}

			GfxRect scissor;

#ifdef RUSH_OPENGL
			scissor.top    = s_window->getHeight() - (int)cmd.ClipRect.w;
			scissor.bottom = s_window->getHeight() - (int)cmd.ClipRect.y;
#else
			scissor.top    = (int)cmd.ClipRect.y;
			scissor.bottom = (int)cmd.ClipRect.w;
#endif

			scissor.left  = (int)cmd.ClipRect.x;
			scissor.right = (int)cmd.ClipRect.z;

			Gfx_SetScissorRect(s_context, scissor);
			s_prim->flush();
			indexOffset += cmd.ElemCount;
		}
	}

	s_prim->end2D();
}

void ImGuiImpl_Startup(Window* window, GfxDevice* device)
{
	RUSH_ASSERT(s_window == nullptr);
	RUSH_ASSERT(s_context == nullptr);

	ImGuiIO& io = ImGui::GetIO();

	s_window = window;
	s_window->setMessageInterceptor(&s_messageInterceptor);

	s_context = Gfx_AcquireContext();

	unsigned char* texData;
	int            texWidth, texHeight;
	io.Fonts->GetTexDataAsRGBA32(&texData, &texWidth, &texHeight);
	s_fontTexture = Gfx_CreateTexture(GfxTextureDesc::make2D(texWidth, texHeight, GfxFormat_RGBA8_Unorm), texData);

	GfxBlendStateDesc blendDesc = GfxBlendStateDesc::makeLerp();
	s_blendState                = Gfx_CreateBlendState(blendDesc);

	GfxRasterizerDesc rasterDesc;
	rasterDesc.cullMode = GfxCullMode::None;
	s_rasterState       = Gfx_CreateRasterizerState(rasterDesc);

	GfxDepthStencilDesc depthDesc;
	depthDesc.enable = false;
	s_depthState     = Gfx_CreateDepthStencilState(depthDesc);

	s_prim = new PrimitiveBatch(24000);

	io.KeyMap[ImGuiKey_Tab]        = Key_Tab;
	io.KeyMap[ImGuiKey_LeftArrow]  = Key_Left;
	io.KeyMap[ImGuiKey_RightArrow] = Key_Right;
	io.KeyMap[ImGuiKey_UpArrow]    = Key_Up;
	io.KeyMap[ImGuiKey_DownArrow]  = Key_Down;
	io.KeyMap[ImGuiKey_Home]       = Key_Home;
	io.KeyMap[ImGuiKey_End]        = Key_End;
	io.KeyMap[ImGuiKey_Delete]     = Key_Delete;
	io.KeyMap[ImGuiKey_Backspace]  = Key_Backspace;
	io.KeyMap[ImGuiKey_Enter]      = Key_Enter;
	io.KeyMap[ImGuiKey_Escape]     = Key_Escape;
	io.KeyMap[ImGuiKey_A]          = Key_A;
	io.KeyMap[ImGuiKey_C]          = Key_C;
	io.KeyMap[ImGuiKey_V]          = Key_V;
	io.KeyMap[ImGuiKey_X]          = Key_X;
	io.KeyMap[ImGuiKey_Y]          = Key_Y;
	io.KeyMap[ImGuiKey_Z]          = Key_Z;

	io.RenderDrawListsFn = ImGuiImpl_Render;
}

void ImGuiImpl_Update(float dt)
{
	ImGuiIO& io = ImGui::GetIO();

	io.DeltaTime = dt;

	io.MousePos.x = s_window->getMouseState().pos.x;
	io.MousePos.y = s_window->getMouseState().pos.y;

	io.MouseDown[0] = s_window->getMouseState().buttons[0];
	io.MouseDown[1] = s_window->getMouseState().buttons[1];

	if (s_window->getWidth() != 0)
		io.DisplaySize.x = (float)s_window->getWidth();

	if (s_window->getHeight() != 0)
		io.DisplaySize.y = (float)s_window->getHeight();

	ImGui::NewFrame();
}

void ImGuiImpl_Shutdown()
{
	ImGui::Shutdown();

	delete s_prim;
	s_prim = nullptr;

	Gfx_Release(s_fontTexture);
	Gfx_Release(s_depthState);
	Gfx_Release(s_rasterState);
	Gfx_Release(s_blendState);

	Gfx_Release(s_context);

	s_context = nullptr;
	s_window  = nullptr;
}
