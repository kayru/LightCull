set(imgui_src
	imgui.cpp
	imgui_demo.cpp
	imgui_draw.cpp
	imconfig.h
	imgui.h
	imgui_internal.h
	stb_rect_pack.h
	stb_textedit.h
	stb_truetype.h
)

add_library(imgui STATIC ${imgui_src})
target_include_directories(imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
