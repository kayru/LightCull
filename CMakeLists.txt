cmake_minimum_required(VERSION 3.7)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

project(LightCull)

set(RUSH_RENDER_API "VK" CACHE STRING "Force Vulkan renderer")

# TODO: bundle prebuilt glslc in this repo for windows and linux
find_program(GLSLC NAMES glslc PATHS
    $ENV{VK_SDK_PATH}/Bin
    $ENV{PATH}
    "~/bin"
)

function(shader_compile_rule shaderName dependencies)
    add_custom_command(
        OUTPUT ${CMAKE_CFG_INTDIR}/${shaderName}.spv
        COMMAND ${GLSLC} -o ${CMAKE_CFG_INTDIR}/${shaderName}.spv ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
        MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
        DEPENDS ${dependencies}
    )
endfunction(shader_compile_rule)

# Enable the AMD Vulkan GLSL/SPIR-V extensions
set(ENABLE_AMD_EXTENSIONS 1)

add_subdirectory("External")
add_subdirectory("Source")
