#pragma once

#include <Rush/Rush.h>

namespace Rush
{
class Window;
class GfxDevice;
}

void ImGuiImpl_Startup(Window* window, GfxDevice* device);
void ImGuiImpl_Update(float dt);
void ImGuiImpl_Shutdown();
