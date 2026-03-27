cmake_minimum_required(VERSION 3.20)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

project(LightCull)

if(APPLE)
	set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "")
	set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO" CACHE STRING "")
endif()

set(RUSH_RENDER_API "VK" CACHE STRING "Force Vulkan renderer")

if(MSVC)
	set(OPT_FLAGS "/O2 /Ob2 /Oi /fp:fast /fp:except- /GS-")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} ${OPT_FLAGS}")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${OPT_FLAGS}")
else()
	set(OPT_FLAGS "-O2 -ffast-math")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${OPT_FLAGS}")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${OPT_FLAGS}")
endif()

set(RUSH_VK_SDK_BIN_PATHS
	$ENV{VULKAN_SDK}/Bin
	$ENV{VULKAN_SDK}/bin
	$ENV{VULKAN_SDK}/macOS/bin
	$ENV{VK_SDK_PATH}/Bin
	$ENV{VK_SDK_PATH}/bin
	$ENV{VK_SDK_PATH}/macOS/bin
	$ENV{PATH}
	"~/bin"
)

find_program(GLSLC NAMES glslc HINTS
	${RUSH_VK_SDK_BIN_PATHS}
)

if(APPLE)
	set(_RUSH_VK_RUNTIME_CANDIDATES
		"$ENV{VULKAN_SDK}/lib/libvulkan.1.dylib"
		"$ENV{VK_SDK_PATH}/lib/libvulkan.1.dylib"
		"$ENV{VULKAN_SDK}/lib/libMoltenVK.dylib"
		"$ENV{VK_SDK_PATH}/lib/libMoltenVK.dylib"
		"$ENV{VULKAN_SDK}/MoltenVK/dylib/macOS/libMoltenVK.dylib"
		"$ENV{VK_SDK_PATH}/MoltenVK/dylib/macOS/libMoltenVK.dylib"
	)

	set(RUSH_VK_RUNTIME_LIBS "")
	foreach(candidate ${_RUSH_VK_RUNTIME_CANDIDATES})
		if(candidate AND EXISTS "${candidate}")
			list(APPEND RUSH_VK_RUNTIME_LIBS "${candidate}")
		endif()
	endforeach()
	if(NOT RUSH_VK_RUNTIME_LIBS)
		message(FATAL_ERROR "Vulkan SDK libraries not found. Set VULKAN_SDK or VK_SDK_PATH.")
	endif()

	set(_RUSH_MOLTENVK_ICD_CANDIDATES
		"$ENV{VULKAN_SDK}/share/vulkan/icd.d/MoltenVK_icd.json"
		"$ENV{VK_SDK_PATH}/share/vulkan/icd.d/MoltenVK_icd.json"
		"$ENV{VULKAN_SDK}/MoltenVK/icd/MoltenVK_icd.json"
		"$ENV{VK_SDK_PATH}/MoltenVK/icd/MoltenVK_icd.json"
	)

	set(RUSH_VK_ICD_JSON "")
	foreach(candidate ${_RUSH_MOLTENVK_ICD_CANDIDATES})
		if(candidate AND EXISTS "${candidate}")
			set(RUSH_VK_ICD_JSON "${candidate}")
			break()
		endif()
	endforeach()
	if(NOT RUSH_VK_ICD_JSON)
		message(FATAL_ERROR "MoltenVK_icd.json not found. Set VULKAN_SDK or VK_SDK_PATH.")
	endif()

	set(RUSH_VK_ICD_RUNTIME "${CMAKE_BINARY_DIR}/MoltenVK_icd_runtime.json")
	file(READ "${RUSH_VK_ICD_JSON}" RUSH_VK_ICD_CONTENT)
	string(REGEX REPLACE "\"library_path\"[ \t]*:[ \t]*\"[^\"]+\"" "\"library_path\": \"libMoltenVK.dylib\"" RUSH_VK_ICD_CONTENT "${RUSH_VK_ICD_CONTENT}")
	file(WRITE "${RUSH_VK_ICD_RUNTIME}" "${RUSH_VK_ICD_CONTENT}")
endif()

function(shader_compile_rule shaderName dependencies)
	add_custom_command(
		OUTPUT ${CMAKE_CFG_INTDIR}/${shaderName}.spv
		COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CFG_INTDIR}/Shaders
		COMMAND ${GLSLC} -o ${CMAKE_CFG_INTDIR}/${shaderName}.spv ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
		MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
		DEPENDS ${dependencies}
	)
endfunction(shader_compile_rule)

# Enable the AMD Vulkan GLSL/SPIR-V extensions
set(ENABLE_AMD_EXTENSIONS 1)

add_subdirectory("External")
add_subdirectory("Source")
