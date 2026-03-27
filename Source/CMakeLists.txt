set(shaderDependencies
	Shaders/Common.glsl
	Shaders/LightingCommon.glsl
	Shaders/LightingTypes.glsl
	Shaders/ShaderDefines.h
	Shaders/TiledLightTreeShadingCommon.glsl
	Shaders/TiledLightTreeTypes.glsl
)

set(shaders
	Shaders/ClusteredShading.comp
	Shaders/FullScreenTriangle.vert
	Shaders/GbufferTranscode.frag
	Shaders/Model.frag
	Shaders/Model.vert
	Shaders/TiledLightTreeShading.comp
	Shaders/TiledLightTreeShadingHybrid.comp
	Shaders/TiledLightTreeShadingMasked.comp
	Shaders/TileStatsDisplay.comp
	Shaders/TileStatsGenerate.comp
	Shaders/TileStatsReduce.comp
)

set(src
	${shaders}
	BaseApplication.cpp
	BaseApplication.h
	ClusteredLightBuilder.cpp
	ClusteredLightBuilder.h
	DiscreteDistribution.h
	EmbeddedAssets.cpp
	EmbeddedAssets.h
	Fiber.cpp
	Fiber.h
	GpuBuffer.h
	ImGuiImpl.cpp
	ImGuiImpl.h
	LightCullApp.cpp
	LightCullApp.h
	LightingCommon.cpp
	LightingCommon.h
	Model.cpp
	Model.h
	Scripting.cpp
	Scripting.h
	Shader.h
	Shader.cpp
	TiledLightTreeBuilder.cpp
	TiledLightTreeBuilder.h
	Utils.cpp
	Utils.h
)

set(app LightCull)

add_executable(${app}
	${src}
)

source_group("Shaders" FILES ${shaders} ${shaderDependencies})

foreach(shader ${shaders})
	shader_compile_rule(${shader} "${shaderDependencies}")
endforeach()

target_compile_definitions(${app} PRIVATE
	RUSH_USING_NAMESPACE # Automatically use Rush namespace
)

target_link_libraries(${app}
	Rush
	imgui
	rapidjson
	stb
	gli
	par_shapes
	enkiTS
)

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")

	target_compile_definitions(${app} PRIVATE USE_FFMPEG=1)

	link_directories("${CMAKE_SOURCE_DIR}/External/assimp/lib")
	target_compile_definitions(${app} PRIVATE USE_ASSIMP=0)
	target_include_directories(${app} PRIVATE "${CMAKE_SOURCE_DIR}/External/assimp/include")
	target_link_libraries(${app}
		${CMAKE_SOURCE_DIR}/External/assimp/lib/assimp-vc140-mt.lib
		${CMAKE_SOURCE_DIR}/External/assimp/lib/zlibstatic.lib
	)
	add_custom_command(TARGET ${app} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${CMAKE_SOURCE_DIR}/External/assimp/bin/assimp-vc140-mt.dll"
		$<TARGET_FILE_DIR:${app}>)

endif()

if(APPLE AND RUSH_VK_RUNTIME_LIBS)
	set(RUSH_VK_RUNTIME_FILES ${RUSH_VK_RUNTIME_LIBS})
	if(RUSH_VK_ICD_RUNTIME)
		list(APPEND RUSH_VK_RUNTIME_FILES "${RUSH_VK_ICD_RUNTIME}")
	endif()

	set_target_properties(${app} PROPERTIES BUILD_RPATH "@executable_path")

	add_custom_command(TARGET ${app} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			${RUSH_VK_RUNTIME_FILES}
			"$<TARGET_FILE_DIR:${app}>"
		COMMENT "Staging MoltenVK runtime"
		VERBATIM
	)

	if(CMAKE_GENERATOR MATCHES "Xcode" AND RUSH_VK_ICD_RUNTIME)
		set_property(TARGET ${app} PROPERTY XCODE_SCHEME_ENVIRONMENT
			"VK_ICD_FILENAMES=$<TARGET_FILE_DIR:${app}>/MoltenVK_icd_runtime.json")
	endif()
endif()
