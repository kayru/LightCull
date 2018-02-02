add_subdirectory("librush")

# Disable warnings in all other external libs
if (MSVC)
	add_definitions(-W0)
else()
	add_definitions(-w)
endif()

add_subdirectory("gli")
add_subdirectory("glm")
add_subdirectory("imgui")
add_subdirectory("par_shapes")
add_subdirectory("rapidjson")
add_subdirectory("stb")
